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
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include <vlc/vlc.h>

#include <libvci/macro.h>
#include <libvci/map.h>
#include <libvci/log.h>

#include "../shared/ipc.h"

#define CLIMP_SERVER_MAX_EPOLL_EVENTS 10
#define CLIMP_SERVER_LOGFILE_PATH "/tmp/climp_server.log"

static libvlc_instance_t *libvlc;
static libvlc_media_player_t *media_player;
static struct message msg_in;
static struct message msg_out;
static struct log log;
static int server_fd;
static int event_fd;
static int epoll_fd;

static void media_player_finished(const struct libvlc_event_t *event, void *arg)
{
    if(event->type == libvlc_MediaPlayerEndReached)
        eventfd_write(event_fd, 1);
}

static int init_vlc(void)
{
    libvlc_event_type_t event;
    libvlc_event_manager_t *manager;
    int err;
    
    libvlc = libvlc_new(0, NULL);
    if(!libvlc) {
        log_error(&log, "libvlc_new(): %s\n", libvlc_errmsg());
        goto out;
    }
    
    media_player = libvlc_media_player_new(libvlc);
    if(!media_player) {
        log_error(&log, "libvlc_media_player_new(): %s\n", libvlc_errmsg());
        goto cleanup1;
    }
    
    manager = libvlc_media_player_event_manager(media_player);
    
    event = libvlc_MediaPlayerEndReached;
    
    err = libvlc_event_attach(manager, event, &media_player_finished, NULL);
    if(err != 0) {
        log_error(&log, "libvlc_event_attach(): %s\n", libvlc_errmsg());
        goto cleanup2;
    }
    
    return 0;
    
cleanup2:
    libvlc_media_player_release(media_player);
cleanup1:
    libvlc_release(libvlc);
out:
    return -1;
}

static void destroy_vlc(void)
{
    if(libvlc_media_player_is_playing(media_player))
        libvlc_media_player_stop(media_player);
    
    libvlc_media_player_release(media_player);
    libvlc_release(libvlc);
}

static int add_input_fd(int fd)
{
    struct epoll_event ev;
    int err;
    
    memset(&ev, 0, sizeof(ev));
    
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    
    err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if(err < 0) {
        log_error(&log, "epoll_ctl(): %s\n", strerror(errno));
        return -errno;
    }
    
    return 0;
}

static int remove_input_fd(int fd)
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
        log_error(&log, "unlink(): %s\n", strerror(errno));
        goto out;
    }
    
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(server_fd < 0) {
        log_error(&log, "socket(): %s\n", strerror(errno));
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CLIMP_IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = bind(server_fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        log_error(&log, "bind(): %s\n", strerror(errno));
        goto cleanup1;
    }
    
    err = listen(server_fd, 5);
    if(err < 0) {
        log_error(&log, "listen(): %s\n", strerror(errno));
        goto cleanup1;
    }
    
    /* Setup eventfd */
    event_fd = eventfd(0, 0);
    if(event_fd < 0) {
        log_error(&log, "eventfd(): %s\n", strerror(errno));
        goto cleanup2;
    }
    
    /* Setup epoll instance */
    epoll_fd = epoll_create(1);
    if(epoll_fd < 0) {
        log_error(&log, "epoll_create(): %s\n", strerror(errno));
        goto cleanup3;
    }
    
    err = add_input_fd(server_fd);
    if(err < 0)
        goto cleanup3;
    
    err = add_input_fd(event_fd);
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
    
    sid = setsid();
    if(sid < 0) {
        err = -errno;
        goto cleanup1;
    }
    
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

static int handle_play(int fd, const struct message *msg)
{
    libvlc_media_t *media;
    int err;
    
    log_info(&log, "%s\n", msg->arg);
    
    media = libvlc_media_new_path(libvlc, msg->arg);
    if(!media) {
        log_error(&log, "libvlc_media_new_path(): %s\n", libvlc_errmsg());
        goto out;
    }
    
    libvlc_media_player_set_media(media_player, media);
    libvlc_media_release(media);
    
    err = libvlc_media_player_play(media_player);
    if(err < 0) {
        log_error(&log, "libvl_media_player_play(): %s\n", libvlc_errmsg());
        goto out;
    }
    
    err = climp_ipc_init_message(&msg_out, CLIMP_MESSAGE_OK, NULL);
    if(err < 0)
        goto out;
    
    err = climp_ipc_send_message(fd, &msg_out);
    if(err < 0)
        goto out;
    
    return 0;
    
out:
    log_error(&log, "play: %s\n", (err == -1) ? "vlc error" : strerror(-err));
    
    err = climp_ipc_init_message(&msg_out, CLIMP_MESSAGE_NO, libvlc_errmsg());
    if(err < 0)
        return err;
    
    err = climp_ipc_send_message(fd, &msg_out);
    if(err < 0)
        return err;
    
    return -EIO;
}

static int handle_stop(int fd, const struct message *msg)
{
    int err;
    
    if(libvlc_media_player_is_playing(media_player))
        libvlc_media_player_stop(media_player);
    
    err = climp_ipc_init_message(&msg_out, CLIMP_MESSAGE_OK, NULL);
    if(err < 0)
        return err;
    
    err = climp_ipc_send_message(fd, &msg_out);
    if(err < 0)
        return err;
    
    return 0;
}

static int handle_hello(int fd, const struct message *msg)
{
    int err;
    
    err = climp_ipc_init_message(&msg_out, CLIMP_MESSAGE_OK, NULL);
    if(err < 0)
        return err;
    
    err = climp_ipc_send_message(fd, &msg_out);
    if(err < 0)
        return err;
    
    log_info(&log, "Registered new client on socket %d\n", fd);
    
    return 0;
}

static int handle_goodbye(int fd, const struct message *msg)
{
    int err;
    
    err = climp_ipc_init_message(&msg_out, CLIMP_MESSAGE_OK, NULL);
    if(err < 0)
        goto out;
    
    err = climp_ipc_send_message(fd, &msg_out);
    if(err < 0)
        goto out;
    
    err = remove_input_fd(fd);
    if(err < 0)
        goto out;
    
    close(fd);
    
    log_info(&log, "Unregistered client at socket %d\n", fd);
    
    return 0;
    
out:
    log_error(&log, "goodbye: %s\n", strerror(-err));
    return err;
}

static int (*message_handlers[CLIMP_MESSAGE_MAX_ID])(int, struct message *) = {
    [CLIMP_MESSAGE_HELLO]   = handle_hello,
    [CLIMP_MESSAGE_GOODBYE] = handle_goodbye,
    [CLIMP_MESSAGE_PLAY]    = handle_play,
    [CLIMP_MESSAGE_STOP]    = handle_stop,
    [CLIMP_MESSAGE_NEXT]    = NULL
};

int main(int argc, char *argv[])
{
    struct epoll_event events[CLIMP_SERVER_MAX_EPOLL_EVENTS];
    eventfd_t val;
    int fd, i, nfds, err;
    
    err = init();
    if(err < 0)
        exit(EXIT_FAILURE);
    
    for(;;) {
        nfds = epoll_wait(epoll_fd, events, ARRAY_SIZE(events), -1);
        if(nfds < 0) {
            log_error(&log, "epoll_wait(): %s\n", strerror(errno));
            continue;
        }
        
        for(i = 0; i < nfds; ++i) {
            if(events[i].data.fd == event_fd) {
                eventfd_read(event_fd, &val);
                
                if(val == 1)
                    goto out;
                
            } else if(events[i].data.fd == server_fd) {
                fd = accept(server_fd, NULL, NULL);
                if(fd < 0) {
                    log_error(&log, "accept(): %s\n", strerror(errno));
                    continue;
                }
                
                err = add_input_fd(fd);
                if(err < 0)
                    continue;
            } else {
                err = climp_ipc_recv_message(events[i].data.fd, &msg_in);
                if(err < 0) {
                    log_error(&log, "climp_ipc_recv_message(): %s\n", strerror(errno));
                    continue;
                }
                
                if(msg_in.id >= CLIMP_MESSAGE_MAX_ID) {
                    log_warning(&log, "received invalid message %d\n", msg_in.id);
                    continue;
                }
                
                err = message_handlers[msg_in.id](events[i].data.fd, &msg_in);
            }
        }
    }
out:
    destroy();
    
    return EXIT_SUCCESS;
}