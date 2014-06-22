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
#include "core/media-manager.h"
#include "core/climpd-player.h"
#include "core/playlist-manager.h"

#include "../../shared/ipc.h"

static const char *tag = "message-handler";
static GIOChannel *io;
static int fd_socket;
static int fd_stdout;
static int fd_stderr;
static struct message msg_in;
static struct message msg_out;
static gboolean socket_handler_run;
static struct clock cl;

extern GMainLoop *main_loop;
extern struct climpd_config conf;

static void out(const char *fmt, ...) __attribute__((format(printf,1,2)));
static void error(const char *fmt, ...) __attribute__((format(printf,1,2)));

static void out(const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    dprintf(fd_stdout, "climpd: ");
    vdprintf(fd_stdout, fmt, vargs);
    
    va_end(vargs);
}

static void error(const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    dprintf(fd_stderr, "climpd: ");
    vdprintf(fd_stderr, fmt, vargs);
    
    va_end(vargs);
}

static void message_handler_close_connection(void)
{
    g_io_channel_shutdown(io, true, NULL);
    
    /* ready for next connection */
    fd_socket = -1;
    
    socket_handler_run = false;
}

static int hello(void)
{
    int fd0, fd1;
    
    fd0 = ipc_message_fd(&msg_in, IPC_MESSAGE_FD_0);
    fd1 = ipc_message_fd(&msg_in, IPC_MESSAGE_FD_1);
    
    ipc_message_clear(&msg_out);
    
    if(fd0 < 0 || fd1 < 0) {
        ipc_message_set_id(&msg_out, IPC_MESSAGE_NO);
        ipc_message_set_arg(&msg_out, strerr(EINVAL));
        
        ipc_send_message(fd_socket, &msg_out);
        
        message_handler_close_connection();
        
        return -EINVAL;
    }
    
    fd_stdout = fd0;
    fd_stderr = fd1;
    
    ipc_message_set_id(&msg_out, IPC_MESSAGE_OK);
    
    return ipc_send_message(fd_socket, &msg_out);
}

static int goodbye(void)
{
    ipc_message_clear(&msg_out);
    ipc_message_set_id(&msg_out, IPC_MESSAGE_OK);
    
    ipc_send_message(fd_socket, &msg_out);
    
    message_handler_close_connection();
    
    return 0;
}

static int do_shutdown(void)
{
    climpd_log_i(tag, "received message to shutdown\n");
    
    g_main_loop_quit(main_loop);
    
    return goodbye();
}

static int mute(void)
{
    bool muted;
    
    /* toggle mute */
    muted = climpd_player_muted();
    climpd_player_set_muted(!muted);
    
    return 0;
}

static int do_pause(void)
{
    climpd_player_pause();
    
    return 0;
}

static int play(void)
{
    int err;
    
    err = climpd_player_play();
    if(err < 0) {
        error("unable to play - %s\n", strerr(-err));
        return err;
    }
    
    climpd_player_print_current_track(fd_stdout);
    
    return 0;
}

static int stop(void)
{
    climpd_player_stop();
    
    return 0;
}

static int get_colors(void)
{
    terminal_color_map_print(fd_stdout);
    
    return 0;
}

static int get_config(void)
{
    climpd_config_print(fd_stdout);
    
    return 0;
}

static int get_files(void)
{
    climpd_player_print_files(fd_stdout);
    
    return 0;
}

static int get_playlists(void)
{
    playlist_manager_print(fd_stdout);
    
    return 0;
}

static int get_state(void)
{
    climpd_player_print_state(fd_stdout);
    
    return 0;
}

static int get_titles(void)
{
    climpd_player_print_playlist(fd_stdout);
    
    return 0;
}

static int get_volume(void)
{
    climpd_player_print_volume(fd_stdout);
    
    return 0;
}

static int get_log(void)
{
    climpd_log_print(fd_stdout);
    
    return 0;
}

static int set_playlist(void)
{
    struct playlist *pl;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&msg_in);
    
    pl = playlist_manager_retrieve(arg);
    if(!pl) {
        err = -errno;
        error("climpd: creating playlist '%s' failed - ", arg);
        error("view the log for details (climp get-log)\n");
        return -err;
    }
    
    climpd_player_set_playlist(pl);
    
    return 0;
}

static int set_repeat(void)
{
    const char *arg;
    const bool *val;
    
    arg = ipc_message_arg(&msg_in);
    
    val = bool_map_value(arg);
    if(!val) {
        error("set-repeat: unknown boolean value %s ", arg);
        error("possbile boolean values are:\n");
        bool_map_print(fd_stderr);
        return -EINVAL;
    }
    
    climpd_player_set_repeat(*val);
    
    return 0;
}

static int set_shuffle(void)
{
    const char *arg;
    const bool *val;
    
    arg = ipc_message_arg(&msg_in);
    
    val = bool_map_value(arg);
    if(!val) {
        error("set-repeat: unknown boolean value %s ", arg);
        error("possbile boolean values are:\n");
        bool_map_print(fd_stderr);
        return -EINVAL;
    }
    
    climpd_player_set_repeat(*val);
    
    return 0;
}

static int set_volume(void)
{
    const char *arg;
    long val;
    int err;
    
    arg = ipc_message_arg(&msg_in);
    
    errno = 0;
    val = strtol(arg, NULL, 10);
    if(errno != 0) {
        error("set-shuffle: invalid value %s - %s\n", arg, strerr(errno));
        err = -errno;
        return err;
    }
    
    climpd_player_set_volume(val);
    
    return 0;
}

static int play_file(void)
{
    struct stat s;
    struct media *m;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&msg_in);
    
    err = stat(arg, &s);
    if(err < 0) {
        error("play-file: %s - %s\n", arg, errstr);
        return -errno;
    }
    
    if(!S_ISREG(s.st_mode)) {
        error("play-file: %s is not a file\n", arg);
        return -EINVAL;
    }
    
    m = media_manager_retrieve(arg);
    if(!m) {
        err = -errno;
        error("failed to create media for %s - %s\n", arg, strerr(errno));
        return err;
    }
    
    err = climpd_player_play_media(m);
    if(err < 0) {
        error("failed to play file %s - %s\n", arg, strerr(-err));
        return -err;
    }
    
    climpd_player_print_current_track(fd_stdout);
    
    return err;
}

static int play_next(void)
{
    int err;
    
    err = climpd_player_next();
    if(err < 0) {
        error("play-next: %s\n", strerr(-err));
        return err;
    }
    
    if(climpd_player_stopped())
        out("play-next: no track available\n");
    else
        climpd_player_print_current_track(fd_stdout);
    
    return 0;
}

static int play_previous(void)
{
    int err;
    
    err = climpd_player_previous();
    if(err < 0) {
        error("play-previous: %s\n", strerr(-err));
        return err;
    }
    
    if(climpd_player_stopped())
        out("play-previous: no track available\n");
    else
        climpd_player_print_current_track(fd_stdout);
    
    return 0;
}

static int play_track(void)
{
    long index;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&msg_in);
    
    errno = 0;
    index = strtol(arg, NULL, 10);
    if(errno != 0)
        return -errno;
    
    err = climpd_player_play_track(index);
    if(err < 0) {
        error("climpd: play-track: %s - %s\n", arg, strerr(-err));
        return err;
    }
    
    climpd_player_print_current_track(fd_stdout);
    
    return 0;
}

static int load_config(void)
{
    int err;
    
    err = climpd_config_reload();
    if(err < 0) {
        climpd_log_e(tag, "climpd_config_reload(): %s\n", strerr(-err));
        error("load-config: %s\n", strerr(-err));
        return err;
    }
    
    climpd_player_set_volume(conf.volume);
    climpd_player_set_repeat(conf.repeat);
    climpd_player_set_shuffle(conf.shuffle);
    
    return 0;
}

static int load_media(void)
{
    struct media *m;
    struct stat s;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(&msg_in);
    
    err = stat(arg, &s);
    if(err < 0) {
        error("load-media: %s - %s\n", arg, errstr);
        return -errno;
    }
    
    if(!S_ISREG(s.st_mode)) {
        error("load-media: %s is not a file\n", arg);
        return -EINVAL;
    }
    
    m = media_manager_retrieve(arg);
    if(err < 0) {
        err = -errno;
        error("load-media %s - %s\n", arg, strerr(-errno));
        return err;
    }
    
    err = climpd_player_insert_media(m);
    if(err < 0) {
        error("load-media: %s - %s\n", arg, strerr(-err));
        return err;
    }
    
    return 0;
}

static int load_playlist(void)
{
    struct playlist *pl;
    const char *arg;
    
    arg = ipc_message_arg(&msg_in);
    
    pl = playlist_manager_retrieve(arg);
    if(!pl) {
        error("climpd: creating playlist '%s' failed - ", arg);
        error("view the log for details (climp get-log)\n");
        return -errno;
    }
    
    return 0;
}

static int remove_media(void)
{
    struct media *m;
    const char *arg;

    arg = ipc_message_arg(&msg_in);
    
    m = media_manager_retrieve(arg);
    if(!m)
        return 0;
    
    climpd_player_take_media(m);

    media_manager_delete_media(arg);
    
    return 0;
}

static int remove_playlist(void)
{
    const char *arg;
    
    arg = ipc_message_arg(&msg_in);
    
    playlist_manager_delete_playlist(arg);
    
    return 0;
}

static int discover(void) 
{
    const char *arg;
    
    arg = ipc_message_arg(&msg_in);
    
    media_manager_discover_directory(arg, fd_stdout);
    
    return 0;
}

static int (*msg_handler[])(void) = {
    [IPC_MESSAGE_HELLO]                 = &hello,
    [IPC_MESSAGE_GOODBYE]               = &goodbye,
    
    [IPC_MESSAGE_SHUTDOWN]              = &do_shutdown,
    
    [IPC_MESSAGE_DISCOVER]              = &discover,
    
    [IPC_MESSAGE_MUTE]                  = &mute,
    [IPC_MESSAGE_PAUSE]                 = &do_pause,
    [IPC_MESSAGE_PLAY]                  = &play,
    [IPC_MESSAGE_STOP]                  = &stop,
    
    [IPC_MESSAGE_GET_COLORS]            = &get_colors,
    [IPC_MESSAGE_GET_CONFIG]            = &get_config,
    [IPC_MESSAGE_GET_FILES]             = &get_files,
    [IPC_MESSAGE_GET_PLAYLISTS]         = &get_playlists,
    [IPC_MESSAGE_GET_STATE]             = &get_state,
    [IPC_MESSAGE_GET_TITLES]            = &get_titles,
    [IPC_MESSAGE_GET_VOLUME]            = &get_volume,
    [IPC_MESSAGE_GET_LOG]               = &get_log,
        
    [IPC_MESSAGE_SET_PLAYLIST]          = &set_playlist,
    [IPC_MESSAGE_SET_REPEAT]            = &set_repeat,
    [IPC_MESSAGE_SET_SHUFFLE]           = &set_shuffle,
    [IPC_MESSAGE_SET_VOLUME]            = &set_volume,
    
    [IPC_MESSAGE_PLAY_FILE]             = &play_file,
    [IPC_MESSAGE_PLAY_NEXT]             = &play_next,
    [IPC_MESSAGE_PLAY_PREVIOUS]         = &play_previous,
    [IPC_MESSAGE_PLAY_TRACK]            = &play_track,
    
    [IPC_MESSAGE_LOAD_CONFIG]           = &load_config,
    [IPC_MESSAGE_LOAD_MEDIA]            = &load_media,
    [IPC_MESSAGE_LOAD_PLAYLIST]         = &load_playlist,
    
    [IPC_MESSAGE_REMOVE_MEDIA]          = &remove_media,
    [IPC_MESSAGE_REMOVE_PLAYLIST]       = &remove_playlist,
};

static gboolean socket_handler(GIOChannel *src, GIOCondition cond, void *data)
{
    enum message_id id;
    const char *msg_id_str;
    unsigned long elapsed;
    int err;
    
    clock_reset(&cl);
    ipc_message_clear(&msg_in);
    
    err = ipc_recv_message(fd_socket, &msg_in);
    if(err < 0) {
        climpd_log_e(tag, "receiving message failed - %s\n", strerr(-err));
        message_handler_close_connection();
        return socket_handler_run;
    }
    
    id = ipc_message_id(&msg_in);
    msg_id_str  = ipc_message_id_string(id);
    
    climpd_log_d(tag, "received '%s'\n", ipc_message_id_string(id));
    
    if(id >= IPC_MESSAGE_MAX_ID) {
        climpd_log_w(tag, "received invalid message id %d\n", id);
        return socket_handler_run;
    }
    
    err = msg_handler[id]();
    
    elapsed = clock_elapsed_ms(&cl);
    
    if(err < 0)
        climpd_log_e(tag, "Message '%s' - %s\n", msg_id_str, strerr(-err));
    else
        climpd_log_i(tag, "handled '%s' in %lu ms\n", msg_id_str, elapsed);
    
    return socket_handler_run;
}

int message_handler_init(void)
{
    int err;
    
    err = clock_init(&cl, CLOCK_MONOTONIC);
    if(err < 0) {
        climpd_log_e(tag, "failed to initialize clock\n");
        return err;
    }
    
    clock_start(&cl);
    
    ipc_message_init(&msg_in);
    ipc_message_init(&msg_out);
    
    io = NULL;
    fd_socket = -1;
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;
}

void message_handler_destroy(void)
{
    close(fd_stderr);
    close(fd_stdout);
    
    if(io)
        g_io_channel_unref(io);
    
    ipc_message_destroy(&msg_out);
    ipc_message_destroy(&msg_in);
    
    clock_destroy(&cl);
    
    climpd_log_i(tag, "destroyed\n");
}


bool message_handler_ready(void)
{
    return fd_socket == -1;
}

int message_handler_add_connection(int fd)
{
    /* clean up previous connection if necessary */
    if(io)
        g_io_channel_unref(io);
    
    io = g_io_channel_unix_new(fd);
    if(!io) {
        climpd_log_e(tag, "unable to create g_io_channel for socket %d\n", fd);
        return -EINVAL;
    }
    g_io_add_watch(io, G_IO_IN, &socket_handler, NULL);
    g_io_channel_set_close_on_unref(io, true);
    socket_handler_run = true;
    
    fd_socket = fd;
    fd_stdout = -1;
    fd_stderr = -1;
    
    climpd_log_i(tag, "ready to handle messages on socket %d\n", fd);

    return 0;
}