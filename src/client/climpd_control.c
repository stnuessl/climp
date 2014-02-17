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

#include "../shared/ipc.h"

#include "climpd_control.h"

extern char **environ;

static int climpd_control_connect(struct climpd_control *__restrict client)
{
    struct sockaddr_un addr;
    int err;
    
    client->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(client->fd < 0) {
        err = -errno;
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CLIMP_IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = connect(client->fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    err = ipc_send_message(client->fd, &client->msg, IPC_MESSAGE_HELLO, NULL);
    if(err < 0)
        goto cleanup1;
    
    err = ipc_recv_message(client->fd, &client->msg);
    if(err < 0)
        goto cleanup1;
    
    if(client->msg.id != IPC_MESSAGE_OK)
        return -EIO;
    
    return 0;
    
cleanup1:
    close(client->fd);
out:
    return err;
}

static int climpd_control_disconnect(struct climpd_control *__restrict client)
{
    int err;
    
    err = ipc_send_message(client->fd, &client->msg, IPC_MESSAGE_GOODBYE, NULL);
    if(err < 0)
        goto out;

out:
    close(client->fd);
    return err;
}

static int climpd_control_spawn_server(void)
{
    char *name = "./climpd";
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

struct climpd_control *climpd_control_new(void)
{
    struct climpd_control *cc;
    int attempts, err;
    
    cc = malloc(sizeof(*cc));
    if(!cc)
        goto out;
    
    err = climpd_control_connect(cc);
    if(err < 0) {
        if(err != -ENOENT && err != -ECONNREFUSED)
            goto cleanup1;
        
        err = unlink(CLIMP_IPC_SOCKET_PATH);
        if(err < 0 && errno != ENOENT) {
            err = -errno;
            goto cleanup1;
        }
        
        err = climpd_control_spawn_server();
        if(err < 0)
            goto cleanup1;
        
        attempts = 10;
        
        while(attempts--) {
            err = climpd_control_connect(cc);
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

void climpd_control_delete(struct climpd_control *__restrict cc)
{
    int err;

    err = climpd_control_disconnect(cc);
    if(err < 0) {
        fprintf(stderr, 
                "climp: client_destroy_instance(): %s\n", 
                strerror(-err));
    }
    
    free(cc);
}


int climpd_control_play(struct climpd_control *__restrict cc,
                        const char *arg)
{
    int err;

    err = ipc_send_message(cc->fd, &cc->msg, IPC_MESSAGE_PLAY, arg);
    if(err < 0)
        goto out;
    
    err = ipc_recv_message(cc->fd, &cc->msg);
    if(err < 0)
        goto out;
    
    if(cc->msg.id == IPC_MESSAGE_NO) {
        fprintf(stderr, "climp: play: %s\n", cc->msg.arg);
        return -EINVAL;
    }
    
    return 0;

out:
    fprintf(stderr, "climp: play: %s\n", strerror(-err));
    return err;
}

int climpd_control_stop(struct climpd_control *__restrict cc)
{
    int err;
    
    err = ipc_send_message(cc->fd, &cc->msg, IPC_MESSAGE_STOP, NULL);
    if(err < 0)
        goto out;
    
    err = ipc_recv_message(cc->fd, &cc->msg);
    if(err < 0)
        goto out;
    
    if(cc->msg.id == IPC_MESSAGE_NO) {
        fprintf(stderr, "climp: stop: %s\n", cc->msg.arg);
        return -EINVAL;
    }
    
    return 0;
    
out:
    fprintf(stderr, "climp: stop: %s\n", strerror(-err));
    return err;
}

int climpd_control_set_volume(struct climpd_control *__restrict cc,
                          const char *arg)
{
    int err;
    
    err = ipc_send_message(cc->fd, &cc->msg, IPC_MESSAGE_VOLUME, arg);
    if(err < 0)
        return err;
    
    err = ipc_recv_message(cc->fd, &cc->msg);
    if(err < 0)
        return err;
    
    if(cc->msg.id == IPC_MESSAGE_NO) {
        fprintf(stderr, "climp: volume: %s\n", cc->msg.arg);
        return -EINVAL;
    }
    
    return 0;
}

int climpd_control_toggle_mute(struct climpd_control *__restrict cc)
{
    int err;
    
    err = ipc_send_message(cc->fd, &cc->msg, IPC_MESSAGE_MUTE, NULL);
    if(err < 0)
        return err;
    
    err = ipc_recv_message(cc->fd, &cc->msg);
    if(err < 0)
        return err;
    
    if(cc->msg.id == IPC_MESSAGE_NO) {
        fprintf(stderr, "climp: volume: %s\n", cc->msg.arg);
        return -EINVAL;
    }
    
    return 0;
}

int climpd_control_shutdown(struct climpd_control *__restrict cc)
{
    return ipc_send_message(cc->fd, &cc->msg, IPC_MESSAGE_SHUTDOWN, NULL);
}

int climpd_control_add(struct climpd_control *__restrict cc, const char *arg)
{
    int err;
    
    err = ipc_send_message(cc->fd, &cc->msg, IPC_MESSAGE_ADD, arg);
    if(err < 0)
        return err;
    
    err = ipc_recv_message(cc->fd, &cc->msg);
    if(err < 0)
        return err;
    
    if(cc->msg.id == IPC_MESSAGE_NO) {
        fprintf(stderr, "climp: add: %s\n", cc->msg.arg);
        return -EINVAL;
    }
    
    return 0;
}

int climpd_control_next(struct climpd_control *__restrict cc)
{
    int err;
    
    err = ipc_send_message(cc->fd, &cc->msg, IPC_MESSAGE_NEXT, NULL);
    if(err < 0)
        return err;
    
    err = ipc_recv_message(cc->fd, &cc->msg);
    if(err < 0)
        return err;
    
    if(cc->msg.id == IPC_MESSAGE_NO) {
        fprintf(stderr, "climp: next: %s\n", cc->msg.arg);
        return -EINVAL;
    }
    
    return 0;
}

int climpd_control_previous(struct climpd_control *__restrict cc)
{
    int err;
    
    err = ipc_send_message(cc->fd, &cc->msg, IPC_MESSAGE_PREVIOUS, NULL);
    if(err < 0)
        return err;
    
    err = ipc_recv_message(cc->fd, &cc->msg);
    if(err < 0)
        return err;
    
    if(cc->msg.id == IPC_MESSAGE_NO) {
        fprintf(stderr, "climp: previous: %s\n", cc->msg.arg);
        return -EINVAL;
    }
    
    return 0;
}