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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include <vlc/vlc.h>

#include <libvci/macro.h>

#include "../shared/ipc.h"

#include "climpd.h"

extern char **environ;

static int climpd_connect(struct climpd *__restrict cc)
{
    struct sockaddr_un addr;
    int err;
    
    cc->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(cc->fd < 0) {
        err = -errno;
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = connect(cc->fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_HELLO);
    ipc_message_set_fds(&cc->msg, STDOUT_FILENO, STDERR_FILENO);
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0) {
        fprintf(stderr, "climp: ipc_send_message(): %s\n", strerror(-err));
        goto cleanup1;
    }
    
    err = ipc_recv_message(cc->fd, &cc->msg);
    if(err < 0) {
        fprintf(stderr, "climp: ipc_recv_message(): %s\n", strerror(-err));
        goto cleanup1;
    }
    
    if(ipc_message_id(&cc->msg) != IPC_MESSAGE_OK) {
        err = -EIO;
        goto cleanup1;
    }
    
    return 0;
    
cleanup1:
    close(cc->fd);
out:
    return err;
}

static int climpd_disconnect(struct climpd *__restrict cc)
{
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_GOODBYE);
    
    ipc_send_message(cc->fd, &cc->msg);
    
    /* This step is needed for a flawless synchronisation */
    ipc_recv_message(cc->fd, &cc->msg);

    close(cc->fd);
    return 0;
}

static int spawn_climpd(void)
{
    char *name = "/usr/local/bin/climpd";
    char *argv[] = { name, NULL };
    pid_t pid;
    
    pid = fork();
    if(pid < 0)
        return -errno;
    
    if(pid > 0)
        return 0;
    
    execve(name, argv, environ);
    
    fprintf(stderr, "climp: execve(): %s\n", strerror(errno));
    
    exit(EXIT_FAILURE);
}

struct climpd *climpd_new(void)
{
    struct climpd *cc;
    int attempts, err;
    
    cc = malloc(sizeof(*cc));
    if(!cc)
        goto out;
    
    ipc_message_init(&cc->msg);
    
    cc->int_err_cnt = 0;
    cc->ext_err_cnt = 0;
    
    err = climpd_connect(cc);
    if(err < 0) {
        if(err != -ENOENT && err != -ECONNREFUSED)
            goto cleanup1;
        
        err = unlink(IPC_SOCKET_PATH);
        if(err < 0 && errno != ENOENT) {
            err = -errno;
            goto cleanup1;
        }
        
        err = spawn_climpd();
        if(err < 0)
            goto cleanup1;
        
        attempts = 10;
        
        while(attempts--) {
            err = climpd_connect(cc);
            if(err < 0) {
                usleep(1000 * 1000);
                continue;
            }
            
            break;
        }
        
        if(err < 0)
            goto cleanup1;
    }

    return cc;
    
cleanup1:
    free(cc);
out:
    return NULL;
}

void climpd_delete(struct climpd *__restrict cc)
{
    int err;

    err = climpd_disconnect(cc);
    if(err < 0) {
        fprintf(stderr, 
                "climp: client_destroy_instance(): %s\n", 
                strerror(-err));
    }
    
    free(cc);
}


void climpd_play(struct climpd *__restrict cc,
                        const char *arg)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_PLAY);
    
    err = ipc_message_set_arg(&cc->msg, arg);
    if(err < 0)
        return;

    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
}

void climpd_stop(struct climpd *__restrict cc)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_STOP);
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
}

void climpd_set_volume(struct climpd *__restrict cc,
                       const char *arg)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_VOLUME);
    
    err = ipc_message_set_arg(&cc->msg, arg);
    if(err < 0)
        return;
    
    err = ipc_message_set_arg(&cc->msg, arg);
    if(err < 0)
        return;
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
}

void climpd_toggle_mute(struct climpd *__restrict cc)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_MUTE);
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
}

void climpd_shutdown(struct climpd *__restrict cc)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_SHUTDOWN);
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
    
    ipc_recv_message(cc->fd, &cc->msg);
}

void climpd_add(struct climpd *__restrict cc, const char *arg)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_ADD);
    
    err = ipc_message_set_arg(&cc->msg, arg);
    if(err < 0)
        return;
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
}

void climpd_next(struct climpd *__restrict cc)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_NEXT);
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
}

void climpd_previous(struct climpd *__restrict cc)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_PREVIOUS);
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
}

void climpd_playlist(struct climpd *__restrict cc, const char *arg)
{
    int err;
    
    ipc_message_clear(&cc->msg);
    ipc_message_set_id(&cc->msg, IPC_MESSAGE_PLAYLIST);
    
    err = ipc_message_set_arg(&cc->msg, arg);
    if(err < 0) {
        fprintf(stdout, "climp: playlist: %s\n", strerror(-err));
        return;
    }
    
    err = ipc_send_message(cc->fd, &cc->msg);
    if(err < 0)
        return;
}