/*
 * Copyright (C) 2014  Steffen Nüssle
 * climp - Command Line Interface Music Player
 *
 * This file is part of climp.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <fcntl.h>
#include <stdint.h>
#include <limits.h>
#include <locale.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <gst/gst.h>

#include <libvci/macro.h>
#include <libvci/error.h>

#include "../shared/ipc.h"
#include "../shared/constants.h"

#include "util/climpd-log.h"
#include "util/bool-map.h"
#include "util/terminal-color-map.h"

#include "core/climpd-player.h"
#include "core/climpd-config.h"
#include "core/climpd-control.h"
#include "core/playlist.h"
#include "core/media.h"

static const char *tag = "main";

static struct climpd_control *cc;
static struct climpd_config *conf;
static int server_fd;

static int close_std_streams(void)
{
    int streams[] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };
    int err, i, fd;
    
    fd = open("/dev/null", O_RDWR);
    if(fd < 0)
        return -errno;
    
    for(i = 0; i < ARRAY_SIZE(streams); ++i) {
        err = dup2(fd, streams[i]);
        if(err < 0) {
            close(fd);
            return -errno;
        }
    }
    
    close(fd);
    return 0;
}

static int init_server_fd(void)
{
    struct sockaddr_un addr;
    int err;
    
    /* Setup Unix Domain Socket */
    err = unlink(IPC_SOCKET_PATH);
    if(err < 0 && errno != ENOENT) {
        err = -errno;
        goto out;
    }
    
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(server_fd < 0) {
        err = -errno;
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = bind(server_fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    err = listen(server_fd, 5);
    if(err < 0) {
        err = -errno;
        goto cleanup1;
    }

    return 0;

cleanup1:
    close(server_fd);
out:
    return err;
}

static void destroy_server_fd(void)
{
    close(server_fd);
    unlink(IPC_SOCKET_PATH);
}

static int init(void)
{
    pid_t sid, pid;
    int err;

    err = climpd_log_init();
    if(err < 0)
        return err;
    
    err = terminal_color_map_init();
    if(err < 0)
        goto cleanup1;
    
    err = bool_map_init();
    if(err < 0)
        goto cleanup2;
    
    climpd_log_i(tag, "starting initialization...\n");
#if 0
    pid = fork();
    if(pid < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    if(pid > 0)
        _exit(EXIT_SUCCESS);
    
    /*
     * We don't want to get closed when our tty is closed.
     * Creating our own session will prevent this.
     */
    sid = setsid();
    if(sid < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    /* 
     * Exit session leading process:
     * Only a session leading process is able to acquire
     * a controlling terminal
     */
    pid = fork();
    if(pid < 0) {
        err = -errno;
       goto cleanup1;
    }
    
    if(pid > 0)
        _exit(EXIT_SUCCESS);
    
#endif
    conf = climpd_config_new(".config/climp/climpd.conf");
    if(!conf) {
        err = -errno;
        climpd_log_e(tag, "climpd_config_new(): %s\n", strerr(-err));
        goto cleanup3;
    }
    
    err = init_server_fd();
    if(err < 0) {
        climpd_log_e(tag, "init_server_fd(): %s\n", strerr(-err));
        goto cleanup4;
    }
    
    cc = climpd_control_new(server_fd, conf);
    if(!cc) {
        err = -errno;
        climpd_log_e(tag, "climpd_control_new(): %s\n", strerr(-err));
        goto cleanup5;
    }
    
    climpd_log_i(tag, "initialization successful\n");
    
    return 0;


cleanup5:
    destroy_server_fd();
cleanup4:
    climpd_config_delete(conf);
cleanup3:
    bool_map_destroy();
cleanup2:
    terminal_color_map_destroy();
cleanup1:

    climpd_log_i(tag, "initialization failed\n");
    climpd_log_destroy();
    
    return err;
}

static void destroy(void)
{
    climpd_control_delete(cc);
    climpd_config_delete(conf);
    destroy_server_fd();
    
    bool_map_destroy();
    terminal_color_map_destroy();
     
    climpd_log_i(tag, "destroyed\n");
    climpd_log_destroy();
}


int main(int argc, char *argv[])
{
    int err;
    
    if(getuid() == 0)
        exit(EXIT_FAILURE);

    setlocale(LC_ALL, "");
    
    gst_init(NULL, NULL);
    
    err = init();
    if(err < 0)
        exit(EXIT_FAILURE);

    climpd_control_run(cc);

    destroy();
    
    gst_deinit();
    
    return EXIT_SUCCESS;
}