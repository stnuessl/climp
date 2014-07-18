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
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gst/gst.h>

#include <libvci/clock.h>
#include <libvci/error.h>

#include "util/climpd-log.h"
#include "util/bool-map.h"
#include "util/terminal-color-map.h"
#include "core/climpd-config.h"
#include "core/climpd-player.h"
#include "core/climpd-control.h"
#include "core/media-discoverer.h"

#include "../../shared/ipc.h"

static const char *tag = "climpd_control";


static void close_connection(struct climpd_control *cc)
{
    g_io_channel_shutdown(cc->io, true, NULL);
    cc->io = NULL;
    
    /* ready for next connection */
    cc->fd_socket = -1;
    cc->socket_handler_run = false;
    
    close(cc->fd_stdout);
    close(cc->fd_stderr);
    
    cc->fd_stdout = -1;
    cc->fd_stderr = -1;
}

static int hello(struct climpd_control *cc)
{
    int fd0, fd1;
    
    fd0 = ipc_message_fd(&cc->msg_in, IPC_MESSAGE_FD_0);
    fd1 = ipc_message_fd(&cc->msg_in, IPC_MESSAGE_FD_1);
    
    ipc_message_clear(&cc->msg_out);
    
    if(fd0 < 0 || fd1 < 0) {
        ipc_message_set_id(&cc->msg_out, IPC_MESSAGE_NO);
        ipc_message_set_arg(&cc->msg_out, strerr(EINVAL));
        
        ipc_send_message(cc->fd_socket, &cc->msg_out);
        
        close_connection(cc);
        
        return -EINVAL;
    }
    
    cc->fd_stdout = fd0;
    cc->fd_stderr = fd1;
    
    ipc_message_set_id(&cc->msg_out, IPC_MESSAGE_OK);
    
    return ipc_send_message(cc->fd_socket, &cc->msg_out);
}

static int goodbye(struct climpd_control *cc)
{
    ipc_message_clear(&cc->msg_out);
    ipc_message_set_id(&cc->msg_out, IPC_MESSAGE_OK);
    
    ipc_send_message(cc->fd_socket, &cc->msg_out);
    
    close_connection(cc);
    
    return 0;
}

static int do_shutdown(struct climpd_control *cc)
{
    climpd_log_i(tag, "received message to shutdown\n");
    
    g_main_loop_quit(cc->main_loop);
    
    return goodbye(cc);
}

static int mute(struct climpd_control *cc)
{
    bool muted;
    
    /* toggle mute */
    muted = climpd_player_muted(&cc->player);
    climpd_player_set_muted(&cc->player, !muted);
    
    return 0;
}

static int do_pause(struct climpd_control *cc)
{
    climpd_player_pause(&cc->player);
    
    return 0;
}

static int play(struct climpd_control *cc)
{
    int err;
    
    err = climpd_player_play(&cc->player);
    if(err < 0) {
        dprintf(cc->fd_stderr, "unable to play - %s\n", strerr(-err));
        return err;
    }
    
    climpd_player_print_running_track(&cc->player, cc->fd_stdout);
    
    return 0;
}

static int stop(struct climpd_control *cc)
{
    climpd_player_stop(&cc->player);
    
    return 0;
}

static int get_colors(struct climpd_control *cc)
{
    terminal_color_map_print(cc->fd_stdout);
    
    return 0;
}

static int get_config(struct climpd_control *cc)
{
    climpd_config_print(cc->conf, cc->fd_stdout);
    
    return 0;
}

static int get_files(struct climpd_control *cc)
{
    climpd_player_print_files(&cc->player, cc->fd_stdout);
    
    return 0;
}

static int get_state(struct climpd_control *cc)
{
    climpd_player_print_state(&cc->player, cc->fd_stdout);
    
    return 0;
}

static int get_playlist(struct climpd_control *cc)
{
    climpd_player_print_playlist(&cc->player, cc->fd_stdout);
    
    return 0;
}

static int get_volume(struct climpd_control *cc)
{
    climpd_player_print_volume(&cc->player, cc->fd_stdout);
    
    return 0;
}

static int get_log(struct climpd_control *cc)
{
    climpd_log_print(cc->fd_stdout);
    
    return 0;
}

static int set_playlist(struct climpd_control *cc)
{
    struct playlist *pl;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&cc->msg_in);
    
    pl = playlist_new_from_file(arg);
    if(!pl) {
        err = -errno;
        dprintf(cc->fd_stderr, "climpd: creating playlist '%s' failed - "
                "view the log for details (climp get-log)\n", arg);
        return -err;
    }
    
    err = climpd_player_set_playlist(&cc->player, pl);
    if(err < 0) {
        playlist_delete(pl);
        dprintf(cc->fd_stderr, "set-playlist: %s\n", strerr(-err));
        return err;
    }
    
    return 0;
}

static int set_repeat(struct climpd_control *cc)
{
    const char *arg;
    const bool *val;
    
    arg = ipc_message_arg(&cc->msg_in);
    
    val = bool_map_value(arg);
    if(!val) {
        dprintf(cc->fd_stderr, "set-repeat: unknown boolean value %s "
                "possbile boolean values are:\n", arg);
        bool_map_print(cc->fd_stderr);
        return -EINVAL;
    }
    
    climpd_player_set_repeat(&cc->player, *val);
    
    return 0;
}

static int set_shuffle(struct climpd_control *cc)
{
    const char *arg;
    const bool *val;
    
    arg = ipc_message_arg(&cc->msg_in);
    
    val = bool_map_value(arg);
    if(!val) {
        dprintf(cc->fd_stderr, "set-repeat: unknown boolean value %s "
                "possbile boolean values are:\n", arg);
        bool_map_print(cc->fd_stderr);
        return -EINVAL;
    }
    
    climpd_player_set_shuffle(&cc->player, *val);
    
    return 0;
}

static int set_volume(struct climpd_control *cc)
{
    const char *arg;
    long val;
    int err;
    
    arg = ipc_message_arg(&cc->msg_in);
    
    errno = 0;
    val = strtol(arg, NULL, 10);
    if(errno != 0) {
        dprintf(cc->fd_stderr, "set-shuffle: invalid value %s - %s\n",
                arg, strerr(errno));
        err = -errno;
        return err;
    }
    
    climpd_player_set_volume(&cc->player, val);
    
    return 0;
}

static int play_file(struct climpd_control *cc)
{
    struct stat s;
    struct media *m;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&cc->msg_in);
    
    err = stat(arg, &s);
    if(err < 0) {
        dprintf(cc->fd_stderr, "play-file: %s - %s\n", arg, errstr);
        return -errno;
    }
    
    if(!S_ISREG(s.st_mode)) {
        dprintf(cc->fd_stderr, "play-file: %s is not a file\n", arg);
        return -EINVAL;
    }
    
    m = media_new(arg);
    if(!m) {
        err = -errno;
        dprintf(cc->fd_stderr, "failed to create media for %s - %s\n", 
                arg, strerr(errno));
        return err;
    }
    
    err = climpd_player_play_media(&cc->player, m);
    if(err < 0) {
        dprintf(cc->fd_stderr, "failed to play file %s - %s\n", 
                arg, strerr(-err));
        return -err;
    }
    
    climpd_player_print_running_track(&cc->player, cc->fd_stdout);
    
    return err;
}

static int next(struct climpd_control *cc)
{
    int err;
    
    err = climpd_player_next(&cc->player);
    if(err < 0) {
        dprintf(cc->fd_stderr, "play-next: %s\n", strerr(-err));
        return err;
    }
    
    if(climpd_player_stopped(&cc->player))
        dprintf(cc->fd_stdout, "play-next: no track available\n");
    else
        climpd_player_print_running_track(&cc->player, cc->fd_stdout);
    
    return 0;
}

static int previous(struct climpd_control *cc)
{
    int err;
    
    err = climpd_player_previous(&cc->player);
    if(err < 0) {
        dprintf(cc->fd_stderr, "play-previous: %s\n", strerr(-err));
        return err;
    }
    
    if(climpd_player_stopped(&cc->player))
        dprintf(cc->fd_stdout, "play-previous: no track available\n");
    else
        climpd_player_print_running_track(&cc->player, cc->fd_stdout);
    
    return 0;
}

static int play_track(struct climpd_control *cc)
{
    long index;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&cc->msg_in);
    
    errno = 0;
    index = strtol(arg, NULL, 10);
    if(errno != 0)
        return -errno;
    
    err = climpd_player_play_track(&cc->player, index);
    if(err < 0) {
        dprintf(cc->fd_stderr, "climpd: play-track: %s - %s\n", arg, 
                strerr(-err));
        return err;
    }
    
    climpd_player_print_running_track(&cc->player, cc->fd_stdout);
    
    return 0;
}

static int load_config(struct climpd_control *cc)
{
    int err;
    
    err = climpd_config_reload(cc->conf);
    if(err < 0) {
        climpd_log_e(tag, "climpd_config_reload(): %s\n", strerr(-err));
        dprintf(cc->fd_stderr, "load-config: %s\n", strerr(-err));
        return err;
    }
    
    climpd_player_set_volume(&cc->player, cc->conf->volume);
    climpd_player_set_repeat(&cc->player, cc->conf->repeat);
    climpd_player_set_shuffle(&cc->player, cc->conf->shuffle);
    
    return 0;
}

static int load_media(struct climpd_control *cc)
{
    struct media *m;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&cc->msg_in);
    
    m = media_new(arg);
    if(!m) {
        err = -errno;
        dprintf(cc->fd_stderr, "load-media: %s - %s\n", arg, strerr(-errno));
        return err;
    }
    
    err = climpd_player_insert_media(&cc->player, m);
    if(err < 0) {
        dprintf(cc->fd_stderr, "load-media: %s - %s\n", arg, strerr(-err));
        return err;
    }
    
    return 0;
}

static int remove_media(struct climpd_control *cc)
{
//     struct media *m;
//     const char *arg;
// 
//     arg = ipc_message_arg(&cc->msg_in);
// 
//     climpd_player_take_media(&cc->player, m);
// 
//     media_delete(m);
    
    return 0;
}

static int remove_playlist(struct climpd_control *cc)
{
    struct playlist *pl;
    int err;
    
    pl = playlist_new("");
    if(!pl) {
        err = -errno;
        dprintf(cc->fd_stderr, "remove-playlist: %s\n", errstr);
        return err;
    }
    
    err = climpd_player_set_playlist(&cc->player, pl);
    if(err < 0) {
        dprintf(cc->fd_stderr, "remove-playlist: %s\n", strerr(-err));
        return err;
    }
    
    return 0;
}

static int discover(struct climpd_control *cc) 
{
    struct media_discoverer md;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&cc->msg_in);

    err = media_discoverer_init(&md, arg, cc->fd_stdout);
    if(err < 0) {
        dprintf(cc->fd_stderr, "discover: %s\n", strerr(-err));
        return err;
    }
    
    media_discoverer_scan(&md);
    media_discoverer_destroy(&md);
    
    return 0;
}

static int (*msg_handler[])(struct climpd_control *cc) = {
    [IPC_MESSAGE_HELLO]                 = &hello,
    [IPC_MESSAGE_GOODBYE]               = &goodbye,
    
    [IPC_MESSAGE_SHUTDOWN]              = &do_shutdown,
    
    [IPC_MESSAGE_DISCOVER]              = &discover,
    
    [IPC_MESSAGE_MUTE]                  = &mute,
    [IPC_MESSAGE_NEXT]                  = &next,
    [IPC_MESSAGE_PAUSE]                 = &do_pause,
    [IPC_MESSAGE_PLAY]                  = &play,
    [IPC_MESSAGE_PREVIOUS]              = &previous,
    [IPC_MESSAGE_STOP]                  = &stop,
    
    [IPC_MESSAGE_GET_COLORS]            = &get_colors,
    [IPC_MESSAGE_GET_CONFIG]            = &get_config,
    [IPC_MESSAGE_GET_FILES]             = &get_files,
    [IPC_MESSAGE_GET_PLAYLIST]          = &get_playlist,
    [IPC_MESSAGE_GET_STATE]             = &get_state,
    [IPC_MESSAGE_GET_VOLUME]            = &get_volume,
    [IPC_MESSAGE_GET_LOG]               = &get_log,
        
    [IPC_MESSAGE_SET_PLAYLIST]          = &set_playlist,
    [IPC_MESSAGE_SET_REPEAT]            = &set_repeat,
    [IPC_MESSAGE_SET_SHUFFLE]           = &set_shuffle,
    [IPC_MESSAGE_SET_VOLUME]            = &set_volume,
    
    [IPC_MESSAGE_PLAY_FILE]             = &play_file,
    [IPC_MESSAGE_PLAY_TRACK]            = &play_track,
    
    [IPC_MESSAGE_LOAD_CONFIG]           = &load_config,
    [IPC_MESSAGE_LOAD_MEDIA]            = &load_media,
    
    [IPC_MESSAGE_REMOVE_MEDIA]          = &remove_media,
    [IPC_MESSAGE_REMOVE_PLAYLIST]       = &remove_playlist,
};

static gboolean socket_handler(GIOChannel *src, GIOCondition cond, void *data)
{
    struct climpd_control *cc;
    enum message_id id;
    const char *msg_id_str;
    unsigned long elapsed;
    int err;
    
    cc = data;
    
    clock_reset(&cc->cl);
    ipc_message_clear(&cc->msg_in);
    
    err = ipc_recv_message(cc->fd_socket, &cc->msg_in);
    if(err < 0) {
        climpd_log_e(tag, "receiving message failed - %s\n", strerr(-err));
        close_connection(cc);
        return cc->socket_handler_run;
    }
    
    id = ipc_message_id(&cc->msg_in);
    msg_id_str  = ipc_message_id_string(id);
    
    climpd_log_d(tag, "received '%s'\n", ipc_message_id_string(id));
    
    if(id >= IPC_MESSAGE_MAX_ID) {
        climpd_log_w(tag, "received invalid message id %d\n", id);
        return cc->socket_handler_run;
    }
    
    err = msg_handler[id](cc);
    
    elapsed = clock_elapsed_ms(&cc->cl);
    
    if(err < 0)
        climpd_log_e(tag, "Message '%s' - %s\n", msg_id_str, strerr(-err));
    else
        climpd_log_i(tag, "handled '%s' in %lu ms\n", msg_id_str, elapsed);
    
    return cc->socket_handler_run;
}

static int add_connection(struct climpd_control *__restrict cc, int fd)
{
    /* clean up previous connection if necessary */
    if(cc->io)
        g_io_channel_unref(cc->io);
    
    cc->io = g_io_channel_unix_new(fd);
    if(!cc->io) {
        climpd_log_e(tag, "unable to create g_io_channel for socket %d\n", fd);
        return -EINVAL;
    }
    g_io_add_watch(cc->io, G_IO_IN, &socket_handler, cc);
    g_io_channel_set_close_on_unref(cc->io, true);
    cc->socket_handler_run = true;
    
    cc->fd_socket = fd;
    cc->fd_stdout = -1;
    cc->fd_stderr = -1;
    
    climpd_log_i(tag, "ready to handle messages on socket %d\n", fd);
    
    return 0;
}


static gboolean handle_server_fd(GIOChannel *src, GIOCondition cond, void *data)
{
    struct climpd_control *cc;
    struct ucred creds;
    socklen_t cred_len;
    int fd, err;
    
    cc = data;
    
    fd = accept(g_io_channel_unix_get_fd(cc->io_server), NULL, NULL);
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
        climpd_log_w(tag, "non-authorized user %d connected -> ", creds.uid);
        climpd_log_append("closing connection\n"); 
        err = -EPERM;
        goto out;
    }
    
    climpd_log_i(tag, "user %d connected on socket %d\n", creds.uid, fd);
    
    if(cc->fd_socket != -1) {
        climpd_log_w(tag, "message-handler not ready - %s\n", strerr(EBUSY));
        goto out;
    }
    
    err = add_connection(cc, fd);
    if(err < 0) {
        climpd_log_w(tag, "failed to add new connection - %s\n", strerr(-err));
        goto out;
    }
    
    return true;
    
    out:
    close(fd);
    return true;
}

struct climpd_control *climpd_control_new(int sock, struct climpd_config *conf)
{
    struct climpd_control *cc;
    int err;
    
    cc = malloc(sizeof(*cc));
    if(!cc)
        return NULL;
    
    err = climpd_control_init(cc, sock, conf);
    if(err < 0) {
        free(cc);
        return NULL;
    }
    
    return cc;
}

void climpd_control_delete(struct climpd_control *__restrict cc)
{
    climpd_control_destroy(cc);
    free(cc);
}

int climpd_control_init(struct climpd_control *__restrict cc, 
                        int sock, 
                        struct climpd_config *conf)
{
    struct playlist *pl;
    int err;
    
    if(conf->default_playlist)
        pl = playlist_new_from_file(conf->default_playlist);
    else
        pl = playlist_new("");
    
    err = climpd_player_init(&cc->player, conf, pl);
    if(err < 0)
        goto cleanup1;
    
    err = clock_init(&cc->cl, CLOCK_MONOTONIC);
    if(err < 0)
        goto cleanup2;
    
    clock_start(&cc->cl);
    
    cc->io_server = g_io_channel_unix_new(sock);
    if(!cc->io_server) {
        err = -EIO;
        goto cleanup3;
    }
    
    g_io_add_watch(cc->io_server, G_IO_IN, &handle_server_fd, cc);
    
    cc->main_loop = g_main_loop_new(NULL, false);
    if(!cc->main_loop) {
        err = -EIO;
        goto cleanup4;
    }
    
    ipc_message_init(&cc->msg_in);
    ipc_message_init(&cc->msg_out);
        
    cc->io = NULL;
    
    cc->conf = conf;
    
    cc->fd_socket = -1;
    cc->fd_stdout = -1;
    cc->fd_stderr = -1;
    
    cc->socket_handler_run = true;

    return 0;

cleanup4:
    g_io_channel_unref(cc->io_server);
cleanup3:
    clock_destroy(&cc->cl);
cleanup2:
    climpd_player_destroy(&cc->player);
cleanup1:
    playlist_delete(pl);
    
    return err;
}

void climpd_control_destroy(struct climpd_control *__restrict cc)
{
    if(cc->fd_stderr != -1)
        close(cc->fd_stderr);
    
    if(cc->fd_stdout != -1)
        close(cc->fd_stdout);
    
    if(cc->io)
        g_io_channel_unref(cc->io);
    
    ipc_message_destroy(&cc->msg_out);
    ipc_message_destroy(&cc->msg_in);
    
    if(g_main_loop_is_running(cc->main_loop))
        g_main_loop_quit(cc->main_loop);
    
    g_main_loop_unref(cc->main_loop);
    g_io_channel_unref(cc->io_server);
    
    clock_destroy(&cc->cl);
    climpd_player_destroy(&cc->player);
}

void climpd_control_run(struct climpd_control *__restrict cc)
{
    g_main_loop_run(cc->main_loop);
}