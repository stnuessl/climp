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
#include <sys/socket.h>
#include <sys/un.h>

#include <gst/gst.h>

#include <libvci/macro.h>
#include <libvci/error.h>

#include "../shared/ipc.h"
#include "../shared/constants.h"

#include "util/bool-map.h"
#include "util/terminal-color-map.h"

#include "core/media-creator.h"
#include "core/climpd-log.h"
#include "core/climpd-player.h"
#include "core/climpd-config.h"
#include "core/playlist-manager.h"
#include "core/message-handler.h"
#include "core/playlist.h"

#include "media-objects/media.h"

extern struct climpd_config conf;

GMainLoop *main_loop;

static const char *tag = "main";
static int server_fd;
static GIOChannel *io_server;

/* TODO: handle bus error */
// static void media_player_handle_bus_error(struct media_player *mp,
//                                           GstMessage *msg)
// {
//     GError *err;
//     gchar *debug_info;
//     const char *name;
//     
//     gst_message_parse_error(msg, &err, &debug_info);
//     
//     name = GST_OBJECT_NAME(msg->src);
//     
//     log_e("Error received from element %s: %s\n", name, err->message);
//     
//     if(debug_info) {
//         log_i("Debugging information: %s\n", debug_info);
//         g_free(debug_info);
//     }
// 
//     g_clear_error(&err);
// }

static gboolean handle_server_fd(GIOChannel *src, GIOCondition cond, void *data)
{
    struct ucred creds;
    socklen_t cred_len;
    int fd, err;
    
    fd = accept(server_fd, NULL, NULL);
    if(fd < 0) {
        err = -errno;
        goto out;
    }
    
    cred_len = sizeof(creds);
    
    err = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &creds, &cred_len);
    if(err < 0) {
        err = -errno;
        climpd_log_e(tag, "getsockopt(): %s\n", strerr(errno));
        goto out;
    }
    
    if(creds.uid != getuid()) {
        climpd_log_w(tag, 
                     "non-authorized user %d connected -> closing connection\n", 
                     creds.uid);
        err = -EPERM;
        goto out;
    }
    
    climpd_log_i(tag, "user %d connected on socket %d\n", creds.uid, fd);
    
    if(!message_handler_ready()) {
        climpd_log_w(tag, "message-handler not ready - %s\n", strerr(EBUSY));
        goto out;
    }
    
    err = message_handler_add_connection(fd);
    if(err < 0) {
        climpd_log_w(tag, "failed to add new connection - %s\n", strerr(-err));
        goto out;
    }

    
    return true;
    
out:
    close(fd);
    return true;
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
    
    io_server = g_io_channel_unix_new(server_fd);
    
    g_io_add_watch(io_server, G_IO_IN, &handle_server_fd, NULL);
    g_io_channel_set_close_on_unref(io_server, true);
    
    return 0;

cleanup1:
    close(server_fd);
out:
    return err;
}

static void destroy_server_fd(void)
{
    g_io_channel_unref(io_server);
    unlink(IPC_SOCKET_PATH);
}

static int init(void)
{
    struct playlist *pl;
    pid_t sid, pid;
    int err;

    err = climpd_log_init();
    if(err < 0)
        goto out;
    
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
    err = close_std_streams();
    if(err < 0) {
        climpd_log_e(tag, "close_std_streams(): %s\n", strerr(-err));
        goto cleanup1;
    }
    
    err = terminal_color_map_init();
    if(err < 0) {
        climpd_log_e(tag, "terminal_color_map_init(): %s\n", strerr(-err));
        goto cleanup1;
    }
    
    err = bool_map_init();
    if(err < 0) {
        climpd_log_e(tag, "bool_map_init(): %s\n", strerr(-err));
        goto cleanup2;
    }
    
    err = climpd_config_init();
    if(err < 0) {
        climpd_log_e(tag, "climpd_config_init(): %s\n", strerr(-err));
        goto cleanup3;
    }
    
    err = media_creator_init();
    if(err < 0) {
        climpd_log_e(tag, "media_creator_init(): %s\n", strerr(-err));
        goto cleanup4;
    }
    
    err = playlist_manager_init();
    if(err < 0) {
        climpd_log_e(tag, "playlist_manager_init(): %s\n", strerr(-err));
        goto cleanup5;
    }
    
    if(conf.playlist_file) {
        err = playlist_manager_load_from_file(conf.playlist_file);
        if(err < 0)
            goto cleanup6;
    }
    
    main_loop = g_main_loop_new(NULL, false);
    if(!main_loop) {
        climpd_log_e(tag, "g_main_loop_new()\n");
        goto cleanup6;
    }
    
    err = init_server_fd();
    if(err < 0) {
        climpd_log_e(tag, "init_server_fd(): %s\n", strerr(-err));
        goto cleanup7;
    }
    
    if(conf.default_playlist) {
        pl = playlist_manager_retrieve(conf.default_playlist);
        if(!pl) {
            pl = playlist_new_file(NULL, conf.default_playlist);
            if(!pl) {
                err = -errno;
                climpd_log_e(tag, 
                             "playlist_new_file(): %s: %s\n", 
                             conf.default_playlist,
                             strerr(-err));
                goto cleanup8;
            }
            
            err = playlist_manager_insert(pl);
            if(err < 0) {
                climpd_log_e(tag,
                             "playlist_manager_insert(): %s: %s\n",
                             playlist_name(pl), strerr(-err));
                
                playlist_delete(pl);
                goto cleanup8;
            }
        }
    } else {
        pl = playlist_manager_retrieve(CLIMPD_PLAYER_DEFAULT_PLAYLIST);
    }
    
    err = climpd_player_init(pl, conf.repeat, conf.shuffle);
    if(err < 0) {
        climpd_log_e(tag, "climp_player_init(): %s\n", strerr(-err));
        goto cleanup8;
    }

    climpd_player_set_volume(conf.volume);
    
    /* TODO: handle bus errors */
//     climp_player_on_bus_error(player, &media_player_handle_bus_error);

    err = message_handler_init();
    if(err < 0) {
        climpd_log_e(tag, "message_handler_init(): %s\n", strerr(-err));
        goto cleanup9;
    }
    
    climpd_log_i(tag, "initialization successful\n");
    
    return 0;

cleanup9:
    climpd_player_destroy();
cleanup8:
    destroy_server_fd();
cleanup7:
    g_main_loop_unref(main_loop);
cleanup6:
    playlist_manager_destroy();
cleanup5:
    media_creator_destroy();
cleanup4:
    climpd_config_destroy();
cleanup3:
    bool_map_destroy();
cleanup2:
    terminal_color_map_destroy();
cleanup1:
    climpd_log_i(tag, "initialization failed\n");
    climpd_log_destroy();
out:
    return err;
}

static void destroy(void)
{
    message_handler_destroy();
    climpd_player_destroy();
    destroy_server_fd();
    g_main_loop_unref(main_loop);
    playlist_manager_destroy();
    media_creator_destroy();
    climpd_config_destroy();
    bool_map_destroy();
    terminal_color_map_destroy();
    climpd_log_destroy();
}


int main(int argc, char *argv[])
{
    int err;
    
    if(getuid() == 0)
        exit(EXIT_FAILURE);
    
    gst_init(NULL, NULL);
    
    err = init();
    if(err < 0)
        exit(EXIT_FAILURE);

    g_main_loop_run(main_loop);

    destroy();
    
    return EXIT_SUCCESS;
}