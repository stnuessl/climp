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
#include <sys/stat.h>
#include <execinfo.h>


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
static int server_fd;

static void handle_error_signal(int signum)
{
#define STACKTRACE_BUFFER_SIZE 32
    static void *buffer[STACKTRACE_BUFFER_SIZE];
    int size;
    
    climpd_log_e(tag, "received signal %s\nbacktrace:\n", strsignal(signum));
    
    size = backtrace(buffer, STACKTRACE_BUFFER_SIZE);
    if(size <= 0)
        return;
    
    backtrace_symbols_fd(buffer, size, climpd_log_fd());
}

static void handle_signal_terminate(int signum)
{
    if(signum != SIGTERM)
        return;
    
    climpd_log_i(tag, "terminating process on signal %s\n", strsignal(signum));
    climpd_control_quit(cc);
}

static void handle_signal_hangup(int signum)
{
    if(signum != SIGHUP)
        return;
    
    climpd_log_i(tag, "reloading config on signal %s\n", strsignal(signum));
    climpd_control_reload_config(cc);
}

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

static int setup_signal_handlers(void)
{
    struct sigaction sa;
    int error_signals[] = { SIGILL, SIGBUS, SIGSEGV, SIGFPE, SIGPIPE, SIGSYS };
    int i, err;
    
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = SIG_IGN;

    err = sigaction(SIGQUIT, &sa, NULL);
    if(err < 0)
        return -errno;
    
    sa.sa_handler = &handle_signal_terminate;
    
    err = sigaction(SIGTERM, &sa, NULL);
    if(err < 0)
        return -errno;
    
    sa.sa_handler = &handle_error_signal;
    
    for(i = 0; i < ARRAY_SIZE(error_signals); ++i) {
        err = sigaction(error_signals[i], &sa, NULL);
        if(err < 0)
            return -errno;
    }
    
    sa.sa_handler = &handle_signal_hangup;
    sigfillset(&sa.sa_mask);
    
    err = sigaction(SIGHUP, &sa, NULL);
    if(err < 0)
        return -errno;
    
    return 0;
}

static int daemonize(void)
{
    int err;
    pid_t pid, sid;
    
    pid = fork();
    if(pid < 0)
        return -errno;
    
    if(pid > 0)
        _exit(EXIT_SUCCESS);
    
    /*
     * We don't want to get closed when our tty is closed.
     * Creating our own session will prevent this.
     */
    sid = setsid();
    if(sid < 0)
        return -errno;
    
    /* 
     * Exit session leading process:
     * Only a session leading process is able to acquire
     * a controlling terminal
     */
    pid = fork();
    if(pid < 0)
        return -errno;
    
    if(pid > 0)
        _exit(EXIT_SUCCESS);
    
    umask(0);
    
    err = chdir("/");
    if(err < 0)
        return -errno;
    
    err = setup_signal_handlers();
    if(err < 0)
        return err;
    
    err = close_std_streams();
    if(err < 0)
        return err;
    
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
    int err;

    err = climpd_log_init();
    if(err < 0)
        return err;
    
    climpd_log_i(tag, "starting initialization...\n");

#ifdef NDEBUG
    err = daemonize();
    if(err < 0) {
        climpd_log_e(tag, "daemonize() - %s\n", strerr(-err));
        goto cleanup1;
    }
#endif
        
    err = terminal_color_map_init();
    if(err < 0) {
        climpd_log_e(tag, "terminal_color_map_init() - %s\n", strerr(-err));
        goto cleanup1;
    }
    
    err = bool_map_init();
    if(err < 0) {
        climpd_log_e(tag, "bool_map_init() - %s\n", strerr(-err));
        goto cleanup2;
    }
    
    err = init_server_fd();
    if(err < 0) {
        climpd_log_e(tag, "init_server_fd(): %s\n", strerr(-err));
        goto cleanup3;
    }
    
    cc = climpd_control_new(server_fd, ".config/climp/climpd.conf");
    if(!cc) {
        err = -errno;
        climpd_log_e(tag, "climpd_control_new(): %s\n", strerr(-err));
        goto cleanup4;
    }
    
    climpd_log_i(tag, "initialization successful\n");
    
    return 0;


cleanup4:
    destroy_server_fd();
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