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

#include <libvci/macro.h>
#include <libvci/log.h>

#include "../shared/ipc.h"
#include "../shared/util.h"

#include "media_player.h"
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
    
#define log_e(func, err)                                                       \
    log_error(&log, "%s: %s at %s:%d\n", (func), (err), __FILE__, __LINE__)

static struct media_player *media_player;
static struct client client;
static struct message msg_in;
static struct message msg_out;
static struct log log;
static int server_fd;
static int event_fd;
static int epoll_fd;
static bool run;

// static void media_player_finished(const struct libvlc_event_t *event, void *arg)
// {
//     if(event->type == libvlc_MediaPlayerEndReached)
//         eventfd_write(event_fd, 1);
// }


static int epoll_add_fd(int fd)
{
    struct epoll_event ev;
    int err;
    
    memset(&ev, 0, sizeof(ev));
    
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    
    err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if(err < 0)
        return -errno;
    
    return 0;
}

static int epoll_remove_fd(int fd)
{
    int err;
    
    err = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if(err < 0)
        return -errno;
    
    return 0;
}

static int init_fds(void)
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
    
    /* Setup eventfd */
    event_fd = eventfd(0, 0);
    if(event_fd < 0) {
        err = -errno;
        goto cleanup2;
    }
    
    /* Setup epoll instance */
    epoll_fd = epoll_create(1);
    if(epoll_fd < 0) {
        err = -errno;
        goto cleanup3;
    }
    
    err = epoll_add_fd(server_fd);
    if(err < 0)
        goto cleanup3;
    
    err = epoll_add_fd(event_fd);
    if(err < 0)
        goto cleanup3;
    
    return 0;
    
cleanup3:
    close(epoll_fd);
    cleanup2:
close(event_fd);
    cleanup1:
close(server_fd);
    unlink(IPC_SOCKET_PATH);
out:
    return err;
}

void destroy_fds(void)
{
    close(epoll_fd);
    close(event_fd);
    close(server_fd);
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

    err = init_fds();
    if(err < 0)
        goto cleanup1;
    

    media_player = media_player_new();
    if(!media_player)
        goto cleanup2;
    
    memset(&client, 0, sizeof(client));
    
    ipc_message_init(&msg_in);
    ipc_message_init(&msg_out);
    
    run = true;
    
    return 0;

cleanup2:
    destroy_fds();
cleanup1:
    log_destroy(&log);
out:
    return err;
}

static void destroy(void)
{
    destroy_fds();
    media_player_delete(media_player);
    log_destroy(&log);
}

static int handle_message_play(int fd, const struct message *msg)
{
    DIR *dir;
    struct dirent *entry;
    struct stat s;
    int err;
    
    err = stat(msg->arg, &s);
    if(err < 0)
        return -errno;
    
    if(S_ISREG(s.st_mode)) {
        err = media_player_add_title(media_player, msg->arg);
        
    } else if(S_ISDIR(s.st_mode)) {

        return 0;
    } else {
        return -EINVAL;
    }
    
    media_player_play(media_player);

    return 0;
}

static int handle_message_stop(int fd, const struct message *msg)
{
    media_player_stop(media_player);
    
    return 0;
}

static int handle_message_next(int fd, const struct message *msg)
{
    media_player_next_title(media_player);
    
    return 0;
}

static int handle_message_previous(int fd, const struct message *msg)
{
    media_player_previous_title(media_player);
    
    return 0;
}

static int handle_message_hello(int fd, const struct message *msg)
{
    client.unix_fd   = fd;
    client.stdout_fd = ipc_message_fd(msg, IPC_MESSAGE_FD_0);
    client.stderr_fd = ipc_message_fd(msg, IPC_MESSAGE_FD_1);
    
    ipc_message_clear(&msg_out);
    
    if(client.stdout_fd < 0 || client.stderr_fd < 0) {
        ipc_message_set_id(&msg_out, IPC_MESSAGE_NO);
        ipc_message_set_arg(&msg_out, errno_string(EINVAL));
        
        ipc_send_message(fd, &msg_out);
        
        epoll_remove_fd(fd);
        close(fd);
        
        return -EINVAL;
    }
    
    ipc_message_set_id(&msg_out, IPC_MESSAGE_OK);
    
    return ipc_send_message(fd, &msg_out);
}

static int handle_message_goodbye(int fd, const struct message *msg)
{
    epoll_remove_fd(fd);
    close(fd);
    
    log_i("Unregistered client at socket %d\n", fd);
    
    return 0;
}

static int handle_message_volume(int fd, const struct message *msg)
{
    const char *err_msg, *arg;
    long val;
    int err;
    
    arg = ipc_message_arg(msg);
    
    errno = 0;
    val = strtol(arg, NULL, 10);
    if(errno != 0) {
        err_msg = errno_string(errno);
        err = -errno;
        return err;
    }
    
    val = min(val, CLIMP_MAX_VOLUME);
    val = max(val, CLIMP_MIN_VOLUME);
    
    return media_player_set_volume(media_player, val);
}

static int handle_message_add(int fd, const struct message *msg)
{
    struct stat s;
    const char *path;
    int err;
    
    path = ipc_message_arg(msg);
    
    err = stat(path, &s);
    if(err < 0)
        return err;
    
    if(S_ISREG(s.st_mode)) {
        err = media_player_add_title(media_player, path);
        
        if(!err) 
            return err;
    } else if(S_ISDIR(s.st_mode)) {
        return 0;
        
    } else {
        return -EINVAL;
    }
    
    media_player_play(media_player);
    
    return 0;
}

static int handle_message_shutdown(int fd, const struct message *msg)
{
    run = false;

    return 0;
}

static int handle_message_mute(int fd, const struct message *msg)
{
    media_player_toggle_mute(media_player);

    return 0;
}

static int handle_message_playlist(int fd, const struct message *msg)
{
    return 0;
}

static int (*msg_handler[IPC_MESSAGE_MAX_ID])(int, const struct message *) = {
    [IPC_MESSAGE_HELLO]    = &handle_message_hello,
    [IPC_MESSAGE_GOODBYE]  = &handle_message_goodbye,
    [IPC_MESSAGE_PLAY]     = &handle_message_play,
    [IPC_MESSAGE_STOP]     = &handle_message_stop,
    [IPC_MESSAGE_NEXT]     = &handle_message_next,
    [IPC_MESSAGE_PREVIOUS] = &handle_message_previous,
    [IPC_MESSAGE_VOLUME]   = &handle_message_volume,
    [IPC_MESSAGE_MUTE]     = &handle_message_mute,
    [IPC_MESSAGE_ADD]      = &handle_message_add,
    [IPC_MESSAGE_SHUTDOWN] = &handle_message_shutdown,
    [IPC_MESSAGE_PLAYLIST] = &handle_message_playlist
};

int main(int argc, char *argv[])
{
    struct epoll_event events[CLIMP_SERVER_MAX_EPOLL_EVENTS];
    eventfd_t val;
    enum message_id id;
    int fd, i, nfds, err;
    
    if(getuid() == 0)
        exit(EXIT_FAILURE);
    
    err = init();
    if(err < 0)
        exit(EXIT_FAILURE);
    
    
     while(run) {
        nfds = epoll_wait(epoll_fd, events, ARRAY_SIZE(events), -1);
        if(nfds < 0) {
            log_e("epoll_wait()", strerror(errno));
            continue;
        }
        
        for(i = 0; i < nfds; ++i) {
            if(events[i].data.fd == event_fd) {
                eventfd_read(event_fd, &val);
                
            } else if(events[i].data.fd == server_fd) {
                fd = accept(server_fd, NULL, NULL);
                if(fd < 0)
                    continue;

                err = epoll_add_fd(fd);
                if(err < 0) {
                    close(fd);
                    continue;
                }
            } else {
                fd = events[i].data.fd;
                
                ipc_message_clear(&msg_in);
                
                err = ipc_recv_message(fd, &msg_in);
                if(err < 0) {
                    log_w("Receiving message on socket %d failed: %s -> "
                          "closing connection...\n", 
                          fd, errno_string(-err));

                    epoll_remove_fd(fd);
                    close(fd);
                    continue; 
                }
                
                id = ipc_message_id(&msg_in);
                
                if(id >= IPC_MESSAGE_MAX_ID) {
                    log_w("Received invalid message\n");
                    continue;
                }
                
    
                err = msg_handler[id](fd, &msg_in);
                if(err < 0) {
                    log_w("Handling message %s failed - %s\n", 
                          ipc_message_id_string(id), strerror(-err));
                }
            }
        }
    }

    destroy();
    
    return EXIT_SUCCESS;
}