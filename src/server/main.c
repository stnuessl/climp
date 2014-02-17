/* 
 * climp - Command Line Interface Music Player
 * Copyright (C) 2014  Steffen Nüssle
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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/stat.h>

#include <vlc/vlc.h>

#include <libvci/macro.h>
#include <libvci/map.h>
#include <libvci/log.h>

#include "../shared/ipc.h"

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

static libvlc_instance_t *libvlc;
static libvlc_media_list_player_t *media_list_player;
static libvlc_media_player_t *media_player;
static libvlc_media_list_t *media_list;
static struct message msg_in;
static struct message msg_out;
static struct log log;
static int server_fd;
static int event_fd;
static int epoll_fd;
static bool run;

static void media_player_finished(const struct libvlc_event_t *event, void *arg)
{
    if(event->type == libvlc_MediaPlayerEndReached)
        eventfd_write(event_fd, 1);
}

static int init_vlc(void)
{
    libvlc_event_type_t ev;
    libvlc_event_manager_t *ev_man;
    int err;
    
    libvlc = libvlc_new(0, NULL);
    if(!libvlc) {
        log_e("libvlc_new()", libvlc_errmsg());
        goto out;
    }
    
    media_player = libvlc_media_player_new(libvlc);
    if(!media_player) {
        log_e("libvlc_media_player_new()", libvlc_errmsg());
        goto cleanup1;
    }
    
    media_list_player = libvlc_media_list_player_new(libvlc);
    if(!media_list_player) {
        log_e("libvlc_media_list_player_new()", libvlc_errmsg());
        goto cleanup2;
    }
    
    media_list = libvlc_media_list_new(libvlc);
    if(!media_list) {
        log_e("libvlc_media_list_new()", libvlc_errmsg());
        goto cleanup3;
    }
    
    libvlc_media_list_player_set_media_player(media_list_player, media_player);
    libvlc_media_list_player_set_media_list(media_list_player, media_list);
    
    ev_man = libvlc_media_player_event_manager(media_player);
    
    ev = libvlc_MediaPlayerEndReached;
    
    err = libvlc_event_attach(ev_man, ev, &media_player_finished, NULL);
    if(err != 0)
        goto cleanup4;
    
    return 0;

cleanup4:
    libvlc_media_list_release(media_list);
cleanup3:
    libvlc_media_list_player_release(media_list_player);
cleanup2:
    libvlc_media_player_release(media_player);
cleanup1:
    libvlc_release(libvlc);
out:
    return -1;
}

static void destroy_vlc(void)
{
    if(libvlc_media_list_player_is_playing(media_list_player))
        libvlc_media_list_player_stop(media_list_player);
    
    libvlc_media_list_player_release(media_list_player);
    libvlc_release(libvlc);
}

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
    err = unlink(CLIMP_IPC_SOCKET_PATH);
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
    strncpy(addr.sun_path, CLIMP_IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
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
    unlink(CLIMP_IPC_SOCKET_PATH);
out:
    return err;
}

void destroy_fds(void)
{
    close(epoll_fd);
    close(event_fd);
    close(server_fd);
    unlink(CLIMP_IPC_SOCKET_PATH);
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
    
    
    err = init_vlc();
    if(err < 0)
        goto cleanup2;
    
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
    destroy_vlc();
    log_destroy(&log);
}

static int handle_message_play(int fd, const struct message *msg)
{
    libvlc_media_t *media;
    const char *err_msg;
    int err;

    media = libvlc_media_new_path(libvlc, msg->arg);
    if(!media) {
        err_msg = libvlc_errmsg();
        log_e("libvlc_media_new_path()", err_msg);
        goto out;
    }
    
    libvlc_media_list_lock(media_list);
    err = libvlc_media_list_add_media(media_list, media);
    libvlc_media_list_unlock(media_list);
    
    libvlc_media_release(media);
    
    if(err < 0) {
        err_msg = libvlc_errmsg();
        log_e("libvlc_media_list_add_media()", err_msg);
        goto out;
    }

    libvlc_media_list_player_play(media_list_player);

    return ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);
    
out:
    ipc_send_message(fd, &msg_out, IPC_MESSAGE_NO, err_msg);
    
    return -EIO;
}

static int handle_message_stop(int fd, const struct message *msg)
{
    if(libvlc_media_list_player_is_playing(media_list_player))
        libvlc_media_list_player_stop(media_list_player);

    return ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);
}

static int handle_message_next(int fd, const struct message *msg)
{
    const char *err_msg;
    int err;
    
    err = libvlc_media_list_player_next(media_list_player);
    if(err < 0) {
        err_msg = libvlc_errmsg();
        log_e("libvlc_media_list_player_next()", err_msg);
        ipc_send_message(fd, &msg_out, IPC_MESSAGE_NO, err_msg);
        return -EIO;
    }
    
    return ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);
}

static int handle_message_previous(int fd, const struct message *msg)
{
    const char *err_msg;
    int err;
    
    err = libvlc_media_list_player_previous(media_list_player);
    if(err < 0) {
        err_msg = libvlc_errmsg();
        log_e("libvlc_media_list_player_previous()", err_msg);
        ipc_send_message(fd, &msg_out, IPC_MESSAGE_NO, err_msg);
        return -EIO;
    }
    
    return ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);
}

static int handle_message_hello(int fd, const struct message *msg)
{
    log_i("Registered new client on socket %d\n", fd);
    
    return ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);
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
    const char *err_msg;
    long val;
    int err;
    
    errno = 0;
    val = strtol(msg->arg, NULL, 10);
    if(errno != 0) {
        err_msg = strerror(errno);
        err = -errno;
        goto out;
    }
    
    val = min(val, CLIMP_MAX_VOLUME);
    val = max(val, CLIMP_MIN_VOLUME);
    
    err = libvlc_audio_set_volume(media_player, val);
    if(err < 0) {
        err_msg = libvlc_errmsg();
        err = -EINVAL;
        goto out;
    }
    
    return ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);;
    
out:
    ipc_send_message(fd, &msg_out, IPC_MESSAGE_NO, err_msg);

    return -EIO;
}

static int handle_message_add(int fd, const struct message *message)
{
    libvlc_media_t *media;
    const char *err_msg;
    int err;
    
    media = libvlc_media_new_path(libvlc, message->arg);
    if(!media) {
        err_msg = libvlc_errmsg();
        log_e("libvlc_media_new_path()", err_msg);
        goto out;
    }
    
    libvlc_media_list_lock(media_list);
    err = libvlc_media_list_add_media(media_list, media);
    libvlc_media_list_unlock(media_list);
    
    libvlc_media_release(media);
    
    if(err < 0) {
        err_msg = libvlc_errmsg();
        log_e("libvlc_media_list_add_media()", err_msg);
        goto out;
    }
    
    if(!libvlc_media_list_player_is_playing(media_list_player))
        libvlc_media_list_player_play(media_list_player);
    
    return ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);
    
out:
    ipc_send_message(fd, &msg_out, IPC_MESSAGE_NO, err_msg);
    
    return -EIO;
}

static int handle_message_shutdown(int fd, const struct message *msg)
{
    ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);
    
    run = false;
    
    return 0;
}

static int handle_message_mute(int fd, const struct message *msg)
{
    libvlc_audio_toggle_mute(media_player);

    return ipc_send_message(fd, &msg_out, IPC_MESSAGE_OK, NULL);
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
    [IPC_MESSAGE_SHUTDOWN] = &handle_message_shutdown
};

int main(int argc, char *argv[])
{
    struct epoll_event events[CLIMP_SERVER_MAX_EPOLL_EVENTS];
    eventfd_t val;
    int fd, i, nfds, err;
    
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
                
                run = !(val == 1);
                
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
                
                err = ipc_recv_message(fd, &msg_in);
                if(err < 0) {
                    log_w("Receiving message on socket %d failed: %s -> "
                          "closing connection...\n", 
                          fd, strerror(-err));

                    epoll_remove_fd(fd);
                    close(fd);
                    continue; 
                }
                
                if(msg_in.id >= IPC_MESSAGE_MAX_ID) {
                    log_w("Received invalid message\n");
                    continue;
                }
                
                err = msg_handler[msg_in.id](fd, &msg_in);
                if(err < 0) {
                    log_w("Handling message %s failed - %s\n", 
                          ipc_message_id_string(msg_in.id), strerror(-err));
                }
            }
        }
    }

    destroy();
    
    return EXIT_SUCCESS;
}