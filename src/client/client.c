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

#include "client.h"

extern char **environ;

static int client_connect(struct client *__restrict client)
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
    
    err = climp_ipc_init_message(&client->msg, CLIMP_MESSAGE_HELLO, NULL);
    if(err < 0)
        goto cleanup1;
    
    err = climp_ipc_send_message(client->fd, &client->msg);
    if(err < 0)
        goto cleanup1;
    
    err = climp_ipc_recv_message(client->fd, &client->msg);
    if(err < 0)
        goto cleanup1;
    
    if(client->msg.id != CLIMP_MESSAGE_OK)
        return -EIO;
    
    return 0;
    
cleanup1:
    close(client->fd);
out:
    return err;
}

static int client_disconnect(struct client *__restrict client)
{
    int err;
    
    err = climp_ipc_init_message(&client->msg, CLIMP_MESSAGE_GOODBYE, NULL);
    if(err < 0)
        goto out;
    
    err = climp_ipc_send_message(client->fd, &client->msg);
    if(err < 0)
        goto out;
    
    err = climp_ipc_recv_message(client->fd, &client->msg);
    if(err < 0)
        goto out;
    
    if(client->msg.id != CLIMP_MESSAGE_OK)
        err = -EIO;
out:
    close(client->fd);
    return err;
}

static int client_spawn_server(void)
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

struct client *client_new(void)
{
    struct client *client;
    int attempts, err;
    
    client = malloc(sizeof(*client));
    if(!client)
        goto out;
    
    err = client_connect(client);
    if(err < 0) {
        if(err != -ENOENT && err != -ECONNREFUSED)
            goto cleanup1;
        
        err = unlink(CLIMP_IPC_SOCKET_PATH);
        if(err < 0 && errno != ENOENT) {
            err = -errno;
            goto cleanup1;
        }
        
        err = client_spawn_server();
        if(err < 0)
            goto cleanup1;
        
        attempts = 10;
        
        while(attempts--) {
            err = client_connect(client);
            if(err < 0) {
                usleep(1000 * 1000);
                continue;
            }
            
            break;
        }
        
        if(err < 0)
            goto cleanup1;
    }

    return client;
    
cleanup1:
    free(client);
out:
    return NULL;
}

void client_delete(struct client *__restrict client)
{
    int err;

    err = client_disconnect(client);
    if(err < 0) {
        fprintf(stderr, 
                "climp: client_destroy_instance(): %s\n", 
                strerror(-err));
    }
    
    free(client);
}


int client_request_play(struct client *__restrict client,
                                const char *arg)
{
    int err;
    
    err = climp_ipc_init_message(&client->msg, CLIMP_MESSAGE_PLAY, arg);
    if(err < 0)
        goto out;
    
    err = climp_ipc_send_message(client->fd, &client->msg);
    if(err < 0)
        goto out;
    
    err = climp_ipc_recv_message(client->fd, &client->msg);
    if(err < 0)
        goto out;
    
    if(client->msg.id == CLIMP_MESSAGE_NO) {
        fprintf(stderr, "climp: play: %s\n", client->msg.arg);
        return -EINVAL;
    }
    
    return 0;

out:
    fprintf(stderr, "climp: play: %s\n", strerror(-err));
    return err;
}

int client_request_stop(struct client *__restrict client)
{
    int err;
    
    err = climp_ipc_init_message(&client->msg, CLIMP_MESSAGE_STOP, NULL);
    if(err < 0)
        goto out;
    
    err = climp_ipc_send_message(client->fd, &client->msg);
    if(err < 0)
        goto out;
    
    err = climp_ipc_recv_message(client->fd, &client->msg);
    if(err < 0)
        goto out;
    
    if(client->msg.id == CLIMP_MESSAGE_NO) {
        fprintf(stderr, "climp: stop: %s\n", client->msg.arg);
        return -EINVAL;
    }
    
    return 0;
    
out:
    fprintf(stderr, "climp: stop: %s\n", strerror(-err));
    return err;
}