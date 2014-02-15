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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include <vlc/vlc.h>

#include <libvci/macro.h>

#define CLIMP_PLAYER_MAX_EPOLL_EVENTS 10
#define CLIMP_PLAYER_MAX_INPUT_LENGTH 256
#define CLIMP_PLAYER_SOCKET_PATH "/tmp/.climp"

static int _sock_fd;
static int _event_fd;
static int _epoll_fd;
static libvlc_instance_t *_libvlc;
static libvlc_media_player_t *_media_player;
static char _buffer[CLIMP_PLAYER_MAX_INPUT_LENGTH];

static int _climp_player_init_fds(void)
{
    struct sockaddr_un addr;
    struct epoll_event event;
    int err;

    /* Setup Unix Domain Socket */
    err = unlink(CLIMP_PLAYER_SOCKET_PATH);
    if(err < 0 && errno != ENOENT) {
        libvlc_printerr("climp: unlink(): %s\n", strerror(errno));
        goto out;
    }

    _sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(_sock_fd < 0) {
        libvlc_printerr("climp: socket(): %s\n", strerror(errno));
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CLIMP_PLAYER_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = bind(_sock_fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        libvlc_printerr("climp: bind(): %s\n", strerror(errno));
        goto cleanup1;
    }
    
    err = listen(_sock_fd, 5);
    if(err < 0) {
        libvlc_printerr("climp: listen: %s\n", strerror(errno));
        goto cleanup1;
    }
    
    /* Setup eventfd */
    _event_fd = eventfd(0, 0);
    if(_event_fd < 0) {
        libvlc_printerr("climp: eventfd(): %s\n", strerror(errno));
        goto cleanup2;
    }
    
    /* Setup epoll instance */
    _epoll_fd = epoll_create(1);
    if(_epoll_fd < 0) {
        libvlc_printerr("climp: epoll_create(): %s\n", strerror(errno));
        goto cleanup3;
    }
    
    memset(&event.data, 0, sizeof(event.data));
    
    event.data.fd = _sock_fd;
    event.events  = EPOLLIN;
    
    err = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event);
    if(err < 0) {
        libvlc_printerr("climp: epoll_ctl(): %s\n", strerror(errno));
        goto cleanup3;
    }
    
    memset(&event.data, 0, sizeof(event.data));
    
    event.data.fd = _event_fd;
    event.events  = EPOLLIN;
    
    err = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event);
    if(err < 0) {
        libvlc_printerr("climp: epoll_ctl(): %s\n", strerror(errno));
        goto cleanup3;
    }
    
    return 0;
    
cleanup3:
    close(_epoll_fd);
cleanup2:
    close(_event_fd);
cleanup1:
    close(_sock_fd);
    unlink(CLIMP_PLAYER_SOCKET_PATH);
out:
    return -1;
}


static void _media_player_finished(const struct libvlc_event_t *event, 
                                   void *arg)
{
    if(event->type == libvlc_MediaPlayerEndReached)
        eventfd_write(_event_fd, 1);
}


static int _climp_player_init_vlc(void)
{
    libvlc_event_type_t event;
    libvlc_event_manager_t *em;
    int err;
    
    _libvlc = libvlc_new(0, NULL);
    if(!_libvlc)
        goto out;
    
    _media_player = libvlc_media_player_new(_libvlc);
    if(!_media_player)
        goto cleanup1;
    
    em = libvlc_media_player_event_manager(_media_player);
    
    event = libvlc_MediaPlayerEndReached;
    
    err = libvlc_event_attach(em, event, &_media_player_finished, NULL);
    if(err != 0)
        goto cleanup2;
    
    return 0;

cleanup2:
    libvlc_media_player_release(_media_player);
cleanup1:
    libvlc_release(_libvlc);
out:
    return -1;
}

int climp_player_init(void)
{
    int err;
    
    err = _climp_player_init_fds();
    if(err < 0)
        return err;
    
    err = _climp_player_init_vlc();
    if(err < 0)
        return err;
    
    return 0;
}

void climp_player_destroy(void)
{
    if(libvlc_media_player_is_playing(_media_player))
        libvlc_media_player_stop(_media_player);
    
    libvlc_media_player_release(_media_player);
    libvlc_release(_libvlc);
}

int climp_player_play_title(const char *title)
{
    libvlc_media_t *media;
    int err;

    media = libvlc_media_new_path(_libvlc, title);
    if(!media)
        return -1;

    libvlc_media_player_set_media(_media_player, media);
    libvlc_media_release(media);

    err = libvlc_media_player_play(_media_player);
    if(err < 0)
        return -1;

    return 0;
}

static int _climp_player_handle_new_connection(void)
{
    struct sockaddr_un addr;
    struct ucred creds;
    socklen_t addr_len, cred_len;
    int fd, err;
    
    memset(&addr, 0, sizeof(addr));
    
    addr_len = sizeof(addr);
    
    fd = accept(_sock_fd, &addr, &addr_len);
    if(fd < 0) {
        libvlc_printerr("climp: epoll_ctl(): %s\n", strerror(errno));
        return -1;
    }
    
    cred_len = sizeof(creds);
    memset(&creds, 0, cred_len);
    
    err = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &creds, &cred_len);
    if(err < 0) {
        libvlc_printerr("climp: epoll_ctl(): %s\n", strerror(errno));
        return -1;
    }
    
    if(getuid() != creds.uid) {
        libvlc_printerr("climp: error: %s\n", strerror(EACCES));
        errno = EACCES;
        return -1;
    }
    
again:
    err = recv(fd, _buffer, sizeof(_buffer), MSG_NOSIGNAL | MSG_WAITALL);
    if(err < 0) {
        if(errno == EINTR)
            goto again;
      
        libvlc_printerr("climp: recv(): %s\n", strerror(errno));
        goto out;
    } else if(err == 0) {
        
        goto out;
    }
    
    fprintf(stdout, "%s\n", _buffer);
    
out:
    close(fd);
    
    return err;
}

int climp_player_handle_events(void)
{
    struct epoll_event events[CLIMP_PLAYER_MAX_EPOLL_EVENTS];
    int i, err, nfds;
    bool run;
    
    do {
        nfds = epoll_wait(_epoll_fd, events, ARRAY_SIZE(events), 0);
        if(nfds < 0) {
            libvlc_printerr("climp: epoll_wait(): %s\n", strerror(errno));
            continue;
        }
        
        for(i = 0; i < nfds; ++i) {
            if(events[i].data.fd == _event_fd) {
                run = false;
            } else if(events[i].data.fd == _sock_fd) {
                err = _climp_player_handle_new_connection();
                if(err < 0)
                    continue;
                
                run = true;
            }
        }

    } while(run);
    
    return 0;
}

const char *climp_error_message(void)
{
    return libvlc_errmsg();
}

int climp_player_send_message(const char *msg)
{
    struct sockaddr_un addr;
    int  fd, err;
    
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) {
        libvlc_printerr("climp: socket(): %s\n", strerror(errno));
        return -1;
    }
    
    /* start with a clean address structure */
    memset(&addr, 0, sizeof(struct sockaddr_un));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CLIMP_PLAYER_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        libvlc_printerr("climp: connect(): %s\n", strerror(errno));
        goto out;
    }
    
again:
    err = send(fd, msg, sizeof(_buffer), MSG_NOSIGNAL);
    if(err < 0) {
        if(errno == EINTR)
            goto again;
        
        libvlc_printerr("climp: send(): %s\n", strerror(errno));
        goto out;
    }
    
out:
    close(fd);
    
    return err;
}