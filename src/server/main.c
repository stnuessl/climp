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
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gst/gst.h>

#include <libvci/macro.h>

#include "../shared/ipc.h"
#include "../shared/util.h"

#include "media-player/playlist.h"
#include "media-player/media.h"

#include "climpd-log.h"
#include "climpd-player.h"
#include "climpd-config.h"
#include "terminal-color-map.h"
#include "bool-map.h"
#include "client.h"

extern struct climpd_config conf;

static struct client client;
static struct message msg_in;
static struct message msg_out;

static int server_fd;
GIOChannel *io_server;
static GMainLoop *main_loop;

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

static int get_playlist(struct client *client, const struct message *msg)
{
    climpd_player_print_playlist(client->stdout);
    
    return 0;
}

static int get_files(struct client *client, const struct message *msg)
{
    climpd_player_print_files(client->stdout);
    
    return 0;
}

static int get_volume(struct client *client, const struct message *msg)
{
    climpd_player_print_volume(client->stdout);
    
    return 0;
}

static int get_state(struct client *client, const struct message *msg)
{
    climpd_player_print_state(client->stdout);
    
    return 0;
}

static int set_state(struct client *client, const struct message *msg)
{
    const char *arg, *err_msg;
    int err;
    
    arg = ipc_message_arg(msg);
    
    if(strcmp("play", arg) == 0) {
        err = climpd_player_play();
        if(err < 0) {
            err_msg = errno_string(-err);
            client_err(client, "climpd: set-status play: %s\n", err_msg);
            return err;
        }
        
        climpd_player_print_current_track(client->stdout);
        
    } else if(strcmp("pause", arg) == 0) {
        climpd_player_pause();
        
    } else if(strcmp("stop", arg) == 0) {
        climpd_player_stop();
        
    } else if(strcmp("mute", arg) == 0) {
        climpd_player_set_muted(true);
        
    } else if(strcmp("unmute", arg) == 0) {
        climpd_player_set_muted(false);
    }
    
    return 0;
}

static int set_playlist(struct client *client, const struct message *msg)
{
    struct playlist *pl;
    const char *arg, *err_msg;
    char *path;
    int err;
    
    arg = ipc_message_arg(msg);
    
    if(arg[0] == '/') {
        pl = playlist_new_file(arg);
    } else {
        path = realpath(arg, NULL);
        if(!path) {
            err = -errno;
            err_msg = errno_string(errno);
            client_err(client, "climpd: set-playlist: %s\n", err_msg);
            return err;
        }
        
        pl = playlist_new_file(path);
        
        free(path);
    }
 
    if(!pl) {
        err = -errno;
        client_err(client, "climpd: set-playlist: %s\n", errno_string(errno));
        return err;
    }
    
    climpd_player_set_playlist(pl);
    
    return 0;
}

static int set_volume(struct client *client, const struct message *msg)
{
    const char *arg;
    long val;
    int err;
    
    arg = ipc_message_arg(msg);
    
    errno = 0;
    val = strtol(arg, NULL, 10);
    if(errno != 0) {
        err = -errno;
        return err;
    }
    
    climpd_player_set_volume(val);
    
    return 0;
}

static int set_repeat(struct client *client, const struct message *msg)
{
    const char *arg;
    const bool *val;
    
    arg = ipc_message_arg(msg);
    
    val = bool_map_value(arg);
    if(!val) {
        client_err(client, "climpd: set-repeat: unknown boolean value %s", arg);
        client_err(client, " - possbile values are:\n");
        bool_map_print(client->stderr);
        return -EINVAL;
    }

    climpd_player_set_repeat(*val);
    
    return 0;
}

static int set_shuffle(struct client *client, const struct message *msg)
{
    const char *arg;
    const bool *val;
    
    arg = ipc_message_arg(msg);
    
    val = bool_map_value(arg);
    if(!val) {
        client_err(client, "climpd: set-shuffle: unknown boolean value %s", arg);
        client_err(client, " - possbile values are:\n");
        bool_map_print(client->stderr);
        return -EINVAL;
    }
    
    climpd_player_set_repeat(*val);
    
    return 0;
}

static int play_next(struct client *client, const struct message *msg)
{
    int err;

    err = climpd_player_next();
    if(err < 0) {
        client_err(client, "climpd: play-next: %s\n", errno_string(-err));
        return err;
    }
    
    /* TODO: check player state */
//     if(climp_player_stopped(player))
//         client_out(client, "climpd: play-next: no track available\n");
//     else
    climpd_player_print_current_track(client->stdout);
    
    return 0;
}

static int play_previous(struct client *client, const struct message *msg)
{
    int err;
    
    err = climpd_player_previous();
    if(err < 0) {
        client_err(client, "climpd: play-previous: %s\n", errno_string(-err));
        return err;
    }
    
    /* TODO: check player state */
//     if(climp_player_stopped(player))
//         client_out(client, "climpd: play-previous: no track available\n");
//     else
    climpd_player_print_current_track(client->stdout);
    
    return 0;
}

static int play_file_realpath(struct client *client, const char *path)
{
    struct stat s;
    int err;
    
    err = stat(path, &s);
    if(err < 0) {
        err = -errno;
        client_err(client, "climpd: play-file: %s - %s\n", 
                   path, errno_string(errno));
        return err;
    }
    
    if(!S_ISREG(s.st_mode)) {
        client_err(client, "climpd: play-file: %s is not a file\n", path);
        return -EINVAL;
    }
    
    err = climpd_player_play_file(path);
    
    climpd_player_print_current_track(client->stdout);
    
    return err;
}

static int play_file(struct client *client, const struct message *msg)
{
    const char *arg;
    char *path;
    int err;
    
    arg = ipc_message_arg(msg);

    if(arg[0] == '/')
        return play_file_realpath(client, arg);
    
    path = realpath(arg, NULL);
    if(!path)
        return -errno;
    
    err = play_file_realpath(client, path);
    
    free(path);
    
    return err;
}

static int play_track(struct client *client, const struct message *msg)
{
    long index;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(msg);
    
    errno = 0;
    index = strtol(arg, NULL, 10);
    if(errno != 0)
        return -errno;
    
    err = climpd_player_play_track(index);
    if(err < 0) {
        client_err(client, "climpd: play-track: %s\n", errno_string(-err));
        return err;
    }
    
    climpd_player_print_current_track(client->stdout);
    
    return 0;
}

static int add_media_realpath(struct client *client, const char *path)
{
    struct stat s;
    int err;
    
    err = stat(path, &s);
    if(err < 0) {
        err = -errno;
        client_err(client, "climpd: %s - %s\n", path, errno_string(errno));
        return err;
    }
    
    if(!S_ISREG(s.st_mode)) {
        client_err(client, "climpd: %s is not a file\n", path);
        return -EINVAL;
    }
    
    err = climpd_player_add_file(path);
    if(err < 0) {
        client_err(client, "climpd: add-file: %s\n", errno_string(-err));
        return err;
    }
    
    return 0;
}

static int add_media(struct client *client, const struct message *msg)
{
    const char *arg;
    char *path;
    int err;
    
    arg = ipc_message_arg(msg);
    
    if(arg[0] == '/')
        return add_media_realpath(client, arg);
    
    path = realpath(arg, NULL);
    if(!path)
        return -errno;
    
    err = add_media_realpath(client, path);
    
    free(path);
    
    return err;
}

static int add_playlist(struct client *client, const struct message *msg)
{
    return 0;
}

static int remove_media(struct client *client, const struct message *msg)
{
    const char *arg;
    char *path;

    arg = ipc_message_arg(msg);
    
    if(arg[0] == '/') {
        climpd_player_delete_file(arg);
        return 0;
    }
        
    path = realpath(arg, NULL);
    if(!path)
        return -errno;
    
    climpd_player_delete_file(path);
    
    free(path);
    
    return 0;
}

static int remove_playlist(struct client *client, const struct message *msg)
{
    return 0;
}

static int handle_message_hello(struct client *client, 
                                const struct message *msg)
{
    int fd, fd0, fd1;
    
    fd  = client_unix_fd(client);
    
    fd0 = ipc_message_fd(msg, IPC_MESSAGE_FD_0);
    fd1 = ipc_message_fd(msg, IPC_MESSAGE_FD_1);
    
    ipc_message_clear(&msg_out);
    
    if(fd0 < 0 || fd1 < 0) {
        ipc_message_set_id(&msg_out, IPC_MESSAGE_NO);
        ipc_message_set_arg(&msg_out, errno_string(EINVAL));
        
        ipc_send_message(fd, &msg_out);
        
        client_destroy(client);
        
        return -EINVAL;
    }
    
    client_set_stdout(client, fd0);
    client_set_stderr(client, fd1);
    
    ipc_message_set_id(&msg_out, IPC_MESSAGE_OK);
    
    return ipc_send_message(fd, &msg_out);
}

static int handle_message_goodbye(struct client *client, 
                                  const struct message *msg)
{
    int fd;
    
    fd = client_unix_fd(client);
    
    ipc_message_clear(&msg_out);
    ipc_message_set_id(&msg_out, IPC_MESSAGE_OK);
    
    ipc_send_message(fd, &msg_out);
    
    climpd_log_i("User %d disconnected on socket %d\n", client->uid, fd);
    
    client_destroy(client);
    
    return 0;
}

static int reload_config(struct client *client, const struct message *msg)
{
    int err;
    
    err = climpd_config_reload();
    if(err < 0) {
        climpd_log_e("climpd_config_reload(): %s\n", errno_string(-err));
        return err;
    }
    
    climpd_player_set_volume(conf.volume);
    climpd_player_set_repeat(conf.repeat);
    climpd_player_set_shuffle(conf.shuffle);
    
    return 0;
}

static int print_config(struct client *client, const struct message *msg)
{
    climpd_config_print(client->stdout);
    
    return 0;
}

// static int handle_message_shutdown(struct client *client, 
//                                    const struct message *msg)
// {
//     g_main_loop_quit(main_loop);
//     
//     return 0;
// }

static int (*msg_handler[])(struct client *, const struct message *) = {
    [IPC_MESSAGE_HELLO]                 = &handle_message_hello,
    [IPC_MESSAGE_GOODBYE]               = &handle_message_goodbye,
    [IPC_MESSAGE_GET_PLAYLIST]          = &get_playlist,
    [IPC_MESSAGE_GET_FILES]             = &get_files,
    [IPC_MESSAGE_GET_VOLUME]            = &get_volume,
    [IPC_MESSAGE_GET_STATE]             = &get_state,
    [IPC_MESSAGE_SET_STATE]             = &set_state,
    [IPC_MESSAGE_SET_PLAYLIST]          = &set_playlist,
    [IPC_MESSAGE_SET_VOLUME]            = &set_volume,
    [IPC_MESSAGE_SET_REPEAT]            = &set_repeat,
    [IPC_MESSAGE_SET_SHUFFLE]           = &set_shuffle,
    [IPC_MESSAGE_PLAY_NEXT]             = &play_next,
    [IPC_MESSAGE_PLAY_PREVIOUS]         = &play_previous,
    [IPC_MESSAGE_PLAY_FILE]             = &play_file,
    [IPC_MESSAGE_PLAY_TRACK]            = &play_track,
    [IPC_MESSAGE_ADD_MEDIA]             = &add_media,
    [IPC_MESSAGE_ADD_PLAYLIST]          = &add_playlist,
    [IPC_MESSAGE_REMOVE_MEDIA]          = &remove_media,
    [IPC_MESSAGE_REMOVE_PLAYLIST]       = &remove_playlist,
    [IPC_MESSAGE_RELOAD_CONFIG]         = &reload_config,
    [IPC_MESSAGE_GET_CONFIG]            = &print_config
};

static gboolean handle_unix_fd(GIOChannel *src, GIOCondition cond, void *data)
{
    enum message_id id;
    int fd, err;
    
    if(cond != G_IO_IN)
        return false;
    
    fd = client_unix_fd(&client);
    
    ipc_message_clear(&msg_in);
    
    err = ipc_recv_message(fd, &msg_in);
    if(err < 0) {
        climpd_log_e("ipc_recv_message(): %s\n", errno_string(-err));
        client_destroy(&client);
        return false;
    }
    
    id = ipc_message_id(&msg_in);
    
    climpd_log_d("Received ' %s '\n", ipc_message_id_string(id));
    
    if(id >= IPC_MESSAGE_MAX_ID) {
        climpd_log_w("Received invalid message id %d\n", id);
        return true;
    }
    
    err = msg_handler[id](&client, &msg_in);
    if(err < 0) {
        climpd_log_e("Message ' %s ': %s\n", 
                     ipc_message_id_string(id), errno_string(-err));

        return true;
    }
    
    climpd_log_d("Handling ' %s ' was successful\n", ipc_message_id_string(id));
    
    if(id == IPC_MESSAGE_GOODBYE)
        return false;
    
    
    return true;
}


static gboolean handle_server_fd(GIOChannel *src, GIOCondition cond, void *data)
{
    struct ucred creds;
    socklen_t cred_len;
    int fd, err;
    
    if(cond != G_IO_IN)
        return false;
    
    fd = accept(server_fd, NULL, NULL);
    if(fd < 0) {
        err = -errno;
        goto out;
    }
    
    cred_len = sizeof(creds);
    
    err = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &creds, &cred_len);
    if(err < 0) {
        err = -errno;
        climpd_log_e("getsockopt(): %s\n", errno_string(errno));
        goto out;
    }
    
    if(creds.uid != getuid()) {
        climpd_log_w("Non authorized user %d tried to connect\n", creds.uid);
        err = -EPERM;
        goto out;
    }
    
    client_init(&client, creds.pid, fd);
    
    client.uid = creds.uid;
 
    g_io_add_watch(client.io, G_IO_IN, &handle_unix_fd, NULL);
    g_io_channel_set_close_on_unref(client.io, true);
    
    climpd_log_i("User %d connected on socket %d\n", creds.uid, fd);
    
    return true;
    
out:
    close(fd);
    return false;
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
    
    climpd_log_i("Starting initialization...\n");
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
        climpd_log_e("close_std_streams(): %s\n", errno_string(-err));
        goto cleanup1;
    }
    
    err = terminal_color_map_init();
    if(err < 0) {
        climpd_log_e("terminal_color_map_init(): %s\n", errno_string(-err));
        goto cleanup1;
    }
    
    err = bool_map_init();
    if(err < 0) {
        climpd_log_e("bool_map_init(): %s\n", errno_string(-err));
        goto cleanup2;
    }
    
    err = climpd_config_init();
    if(err < 0) {
        climpd_log_e("climpd_config_init(): %s\n", errno_string(-err));
        goto cleanup3;
    }
    
    main_loop = g_main_loop_new(NULL, false);
    if(!main_loop) {
        climpd_log_e("g_main_loop_new()\n");
        goto cleanup4;
    }
    
    err = init_server_fd();
    if(err < 0) {
        climpd_log_e("init_server_fd(): %s\n", errno_string(-err));
        goto cleanup5;
    }
    
    err = climpd_player_init();
    if(err < 0) {
        climpd_log_e("climp_player_init(): %s\n", errno_string(errno));
        goto cleanup6;
    }

    climpd_player_set_volume(conf.volume);
    climpd_player_set_repeat(conf.repeat);
    climpd_player_set_shuffle(conf.shuffle);
    
    if(conf.default_playlist) {
        pl = playlist_new_file(conf.default_playlist);
        if(!pl)
            climpd_log_e("Default playlist failed: %s\n", errno_string(errno));
        else
            climpd_player_set_playlist(pl);
    }
    
    /* TODO: handle bus errors */
//     climp_player_on_bus_error(player, &media_player_handle_bus_error);

    memset(&client, 0, sizeof(client));
    
    ipc_message_init(&msg_in);
    ipc_message_init(&msg_out);
    
    climpd_log_i("Initialization successful\n");
    
    return 0;

    
cleanup6:
    destroy_server_fd();
cleanup5:
    g_main_loop_unref(main_loop);
cleanup4:
    climpd_config_destroy();
cleanup3:
    bool_map_destroy();
cleanup2:
    terminal_color_map_destroy();
cleanup1:
    climpd_log_i("Initialization failed\n");
    climpd_log_destroy();
out:
    return err;
}

static void destroy(void)
{
    climpd_player_destroy();
    destroy_server_fd();
    g_main_loop_unref(main_loop);
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