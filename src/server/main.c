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
#include <libvci/log.h>

#include "../shared/ipc.h"
#include "../shared/util.h"

#include "media_player/media_player.h"
#include "media_player/playlist.h"
#include "media_player/media.h"

#include "color.h"
#include "client.h"

#define CLIMP_SERVER_MAX_EPOLL_EVENTS   10
#define CLIMP_SERVER_LOGFILE_PATH       "/tmp/climpd.log"
#define CLIMP_MIN_VOLUME                0
#define CLIMP_MAX_VOLUME                120

#define log_i(format, ...)                                                     \
    log_info(&log, format, ##__VA_ARGS__)
    
#define log_m(format, ...)                                                     \
    log_message(&log, format, ##__VA_ARGS__)
    
#define log_w(format, ...)                                                     \
    log_warning(&log, format, ##__VA_ARGS__)

#define log_e(format, ...)                                                     \
    log_error(&log, format, ##__VA_ARGS__)

#define log_func_e(func, err)                                                  \
    log_error(&log, "%s: %s at %s:%d\n", (func), (err), __FILE__, __LINE__)

unsigned int media_meta_length = 24;
const char *current_media_meta_color = COLOR_GREEN;
const char *media_meta_color = COLOR_DEFAULT;


static struct media_player *media_player;
static struct client client;
static struct message msg_in;
static struct message msg_out;
static struct log log;
static int server_fd;
GIOChannel *io_server;
static GMainLoop *main_loop;

static void media_player_handle_bus_error(struct media_player *mp,
                                          GstMessage *msg)
{
    GError *err;
    gchar *debug_info;
    const char *name;
    
    gst_message_parse_error(msg, &err, &debug_info);
    
    name = GST_OBJECT_NAME(msg->src);
    
    log_e("Error received from element %s: %s\n", name, err->message);
    
    if(debug_info) {
        log_i("Debugging information: %s\n", debug_info);
        g_free(debug_info);
    }

    g_clear_error(&err);
}

static int get_playlist(struct client *client, const struct message *msg)
{
    client_print_media_player_playlist(client, media_player);
    
    return 0;
}

static int get_files(struct client *client, const struct message *msg)
{
    struct playlist *pl;
    struct media *m;
    struct link *link;
    
    pl = media_player_playlist(media_player);
    
    playlist_for_each(pl, link) {
        m = container_of(link, struct media, link);
        
        client_out(client, "%s\n", m->path);
    }
    
    return 0;
}

static int get_volume(struct client *client, const struct message *msg)
{
    unsigned int vol;
    
    vol = media_player_volume(media_player);
    
    client_print_volume(client, vol);
    
    return 0;
}

static int get_status(struct client *client, const struct message *msg)
{
    return 0;
}

static int set_status(struct client *client, const struct message *msg)
{
    const char *arg;
    int err;
    
    arg = ipc_message_arg(msg);
    
    if(strcmp("play", arg) == 0) {
        err = media_player_play(media_player);
        if(err < 0)
            return err;
        
    } else if(strcmp("pause", arg) == 0) {
        media_player_pause(media_player);
        
    } else if(strcmp("stop", arg) == 0) {
        media_player_stop(media_player);
        
    } else if(strcmp("mute", arg) == 0) {
        media_player_mute(media_player);
        
    } else if(strcmp("unmute", arg) == 0) {
        media_player_unmute(media_player);
        
    }
    
    return 0;
}

static int set_playlist(struct client *client, const struct message *msg)
{
    struct playlist *pl;
    FILE *file;
    const char *arg;
    char *buf;
    size_t size;
    ssize_t n;
    int err;
    
    arg = ipc_message_arg(msg);
    
    file = fopen(arg, "r");
    if(!file) {
        err = -errno;
        client_err(client, "climpd: %s: %s\n", arg, errno_string(errno));
        return err;
    }
    
    pl = media_player_playlist(media_player);
    
    size = 0;
    buf = NULL;
    
    while(1) {
        n = getline(&buf, &size, file);
        if(n < 0)
            break;
        
        buf[n - 1] = '\0';
        
        if(buf[0] != '/') {
            client_err(client, 
                       "climpd: unable to add %s - absolute file path needed\n",
                       buf);
            
            continue;
        }
            
        err = playlist_add_file(pl, buf);
        if(err < 0) {
            client_err(client, "climpd: unable to add %s: %s\n",
                       buf, errno_string(-err));
        }
    }
    
    free(buf);
    
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
    
    media_player_set_volume(media_player, val);
    
    return 0;
}

static int play_next(struct client *client, const struct message *msg)
{
    struct playlist *pl;
    struct media *m;
    int err;
    
    pl = media_player_playlist(media_player);
    
    m = playlist_next(pl, media_player_current_media(media_player));
    if(!m) {
        client_err(client, "climpd: no next track available\n");
        return -EINVAL;
    }
    
    err = media_player_play_media(media_player, m);
    if(err < 0)
        return err;
    
    client_print_current_media(client, media_player);
    
    return 0;
}

static int play_previous(struct client *client, const struct message *msg)
{
    struct playlist *pl;
    struct media *m;
    int err;
    
    pl = media_player_playlist(media_player);
    
    m = playlist_previous(pl, media_player_current_media(media_player));
    if(!m) {
        client_err(client, "climpd: no previous track available\n");
        return -EINVAL;
    }
    
    err = media_player_play_media(media_player, m);
    if(err < 0)
        return err;
    
    client_print_current_media(client, media_player);
    
    return 0;
}

static int play_file_realpath(struct client *client, const char *path)
{
    struct media *media;
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
    
    media = media_new(path);
    if(!media)
        return- errno;
    
    err = media_player_play_media(media_player, media);
    if(err < 0) {
        client_err(client, "climpd: Unable to play %s: %s",
                   path, errno_string(-err));
        
        return err;
    }
    
    media = media_player_current_media(media_player);
    
    client_print_current_media(client, media_player);
    
    return 0;
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
    struct playlist *pl;
    struct media *m;
    long index;
    const char *arg;
    int err;
    
    arg = ipc_message_arg(msg);
    
    errno = 0;
    index = strtol(arg, NULL, 10);
    if(errno != 0)
        return -errno;
    
    pl = media_player_playlist(media_player);
    
    m = playlist_at(pl, index);
    if(!m)
        return -EINVAL;
    
    err = media_player_play_media(media_player, m);
    if(err < 0)
        return err;
    
    client_print_current_media(client, media_player);
    
    return 0;
}

static int add_media_realpath(struct client *client, const char *path)
{
    struct playlist *pl;
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
    
    pl = media_player_playlist(media_player);
    
    err = playlist_add_file(pl, path);
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

static int remove_media_realpath(struct client *client, const char *path)
{
    struct playlist *pl;
    struct media *m, *current;
    int err;
    
    current = media_player_current_media(media_player);
    pl = media_player_playlist(media_player);
    
    if(!media_is_from_file(current, path)) {
        playlist_remove_file(pl, path);
        return 0;
    }
    
    m = playlist_next(pl, current);
    
    err = media_player_play_media(media_player, m);
    
    playlist_delete_media(pl, m);
    
    client_print_current_media(client, media_player);
    
    return err;
}

static int remove_media(struct client *client, const struct message *msg)
{
    const char *arg;
    char *path;
    int err;

    arg = ipc_message_arg(msg);
    
    if(arg[0] == '/')
        return remove_media_realpath(client, arg);
        
    path = realpath(arg, NULL);
    if(!path)
        return -errno;
    
    err = remove_media_realpath(client, path);
    
    free(path);
    
    return err;
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
    
    client_set_out_fd(client, fd0);
    client_set_err_fd(client, fd1);
    
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
    
    client_destroy(client);
    
    log_i("Unregistered process %d at socket %d\n", 
          client->pid, fd);
    
    return 0;
}

// static int handle_message_add(struct client *client, const struct message *msg)
// {
//     struct playlist *playlist;
//     struct stat s;
//     const char *path;
//     int err;
//     
//     path = ipc_message_arg(msg);
//     
//     err = stat(path, &s);
//     if(err < 0)
//         return err;
//     
//     if(!S_ISREG(s.st_mode)) {
//         client_err(client, "climpd: %s is not a file\n", path);
//         return -EINVAL; 
//     }
//     
//     playlist = media_player_playlist(media_player);
//     
//     err = playlist_add_file(playlist, path);
//     if(err < 0) {
//         client_err(client, "climpd: unable to add %s - %s\n", 
//                    path, errno_string(-err));
//         return err;
//     }
//     
//     return 0;
// }

// static int handle_message_shutdown(struct client *client, 
//                                    const struct message *msg)
// {
//     g_main_loop_quit(main_loop);
//     
//     return 0;
// }

static int (*msg_handler[])(struct client *, const struct message *) = {
    [IPC_MESSAGE_HELLO]         = &handle_message_hello,
    [IPC_MESSAGE_GOODBYE]       = &handle_message_goodbye,
    [IPC_MESSAGE_GET_PLAYLIST]  = &get_playlist,
    [IPC_MESSAGE_GET_FILES]     = &get_files,
    [IPC_MESSAGE_GET_VOLUME]    = &get_volume,
    [IPC_MESSAGE_GET_STATUS]    = &get_status,
    [IPC_MESSAGE_SET_STATUS]    = &set_status,
    [IPC_MESSAGE_SET_PLAYLIST]  = &set_playlist,
    [IPC_MESSAGE_SET_VOLUME]    = &set_volume,
    [IPC_MESSAGE_PLAY_NEXT]     = &play_next,
    [IPC_MESSAGE_PLAY_PREVIOUS] = &play_previous,
    [IPC_MESSAGE_PLAY_FILE]     = &play_file,
    [IPC_MESSAGE_PLAY_TRACK]    = &play_track,
    [IPC_MESSAGE_ADD_MEDIA]     = &add_media,
    [IPC_MESSAGE_REMOVE_MEDIA]  = &remove_media
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
        log_w("ipc_recv_message() on socket %d failed: %s -> "
        "closing connection...\n", 
        fd, errno_string(-err));
        
        client_destroy(&client);
        return false;
    }
    
    id = ipc_message_id(&msg_in);
    
    if(id >= IPC_MESSAGE_MAX_ID) {
        log_w("Received invalid message with id%d\n", id);
        return false;
    }
    
    err = msg_handler[id](&client, &msg_in);
    if(err < 0) {
        log_w("Handling message %s failed - %s\n", 
              ipc_message_id_string(id), strerror(-err));
        return false;
    }
    
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
        log_func_e("getsockopt()", errno_string(errno));
        goto out;
    }
    
    if(creds.uid != getuid()) {
        log_w("Non authorized user %d tried to connect\n", creds.uid);
        err = -EPERM;
        goto out;
    }
    
    client_init(&client, creds.pid, fd);
 
    g_io_add_watch(client.io, G_IO_IN, &handle_unix_fd, NULL);
    g_io_channel_set_close_on_unref(client.io, true);
    
    log_i("New connection on socket %d with process %d\n", fd, creds.pid);
    
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
    pid_t sid, pid;
    int err;
    uint8_t flags;
    
    flags = LOG_PRINT_DATE | 
            LOG_PRINT_TIMESTAMP | 
            LOG_PRINT_PID | 
            LOG_PRINT_NAME | 
            LOG_PRINT_HOSTNAME | 
            LOG_PRINT_LEVEL;
    
    err = log_init(&log, CLIMP_SERVER_LOGFILE_PATH, "climpd", flags);
    if(err < 0)
        goto out;
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
    if(err < 0)
        goto cleanup1;
    
    main_loop = g_main_loop_new(NULL, false);
    if(!main_loop)
        goto cleanup1;
    
    err = init_server_fd();
    if(err < 0)
        goto cleanup2;
    
    media_player = media_player_new();
    if(!media_player)
        goto cleanup3;
    
    media_player_on_bus_error(media_player, &media_player_handle_bus_error);

    memset(&client, 0, sizeof(client));
    
    ipc_message_init(&msg_in);
    ipc_message_init(&msg_out);
    
    return 0;

cleanup3:
    destroy_server_fd();
cleanup2:
    g_main_loop_unref(main_loop);
cleanup1:
    log_destroy(&log);
out:
    return err;
}

static void destroy(void)
{
    media_player_delete(media_player);
    destroy_server_fd();
    g_main_loop_unref(main_loop);
    log_destroy(&log);
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