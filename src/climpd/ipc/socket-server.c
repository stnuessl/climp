/*
 * Copyright (C) 2015  Steffen Nüssle
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
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <libvci/error.h>

#include <core/climpd-log.h>
#include <ipc/socket-server.h>


static const char *tag = "socket-server";

static gboolean handle_socket(GIOChannel *src, GIOCondition cond, void *data)
{
    struct socket_server *ss = data;
    struct ucred creds;
    socklen_t cred_len;
    int fd, err;
    
    (void) src;
    (void) cond;
    
    clock_reset(&ss->timer);
    
    fd = accept(g_io_channel_unix_get_fd(ss->channel), NULL, NULL);
    if(fd < 0) {
        err = -errno;
        goto out;
    }
    
    cred_len = sizeof(creds);
    
    err = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &creds, &cred_len);
    if(err < 0) {
        err = -errno;
        climpd_log_e(tag, "getsockopt(): %s\n", strerr(errno));
        goto out;
    }
    
    if(creds.uid != getuid()) {
        climpd_log_w(tag, "non-authorized user %d connected -> closing "
        "connection\n", creds.uid);
        err = -EPERM;
        goto out;
    }
    
    climpd_log_i(tag, "user %d connected on socket %d\n", creds.uid, fd);
    
    if (ss->connection_handler) {
        err = ss->connection_handler(fd);
        if (err < 0) {
            climpd_log_e(tag, "handling connection on socket %d failed - %s\n",
                         fd, strerr(-err));
            goto out;
        }  
    } else {
        climpd_log_w(tag, "connection handler not set\n");
    }
    
out:
    close(fd);
    
    climpd_log_i(tag, "served connection on socket %d in %lu ms\n", fd, 
                 clock_elapsed_ms(&ss->timer));
    
    return true;
}

int socket_server_init(struct socket_server *__restrict ss,
                       const char *__restrict path,
                       int (*handler)(int fd))
{
    struct sockaddr_un addr;
    int fd, err;
    
    err = clock_init(&ss->timer, CLOCK_MONOTONIC);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize timer - %s\n", strerr(-err));
        return err;
    }
    
    clock_start(&ss->timer);
    
    ss->path = strdup(path);
    if (!ss->path) {
        climpd_log_e(tag, "failed to allocate memory - %s\n", errstr);
        goto cleanup1;
    }
    
    err = unlink(ss->path);
    if (err < 0 && errno != ENOENT) {
        climpd_log_e(tag, "failed to remove old socket '%s' - %s\n",
                     path, strerr(errno));
        goto cleanup2;
    }
    
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        climpd_log_e(tag, "failed to create socket '%s' - %s\n", path, errstr);
        goto cleanup2;
    }
    
    addr.sun_family = AF_UNIX;
    *stpncpy(addr.sun_path, ss->path, sizeof(addr.sun_path)) = '\0';
    
    err = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    if (err < 0) {
        climpd_log_e(tag, "failed to bind socket '%s' - %s\n", ss->path, 
                     errstr);
        goto cleanup3;
    }
    
    err = listen(fd, 5);
    if (err < 0) {
        climpd_log_e(tag, "failed to listen on socket '%s' - %s\n", ss->path,
                     errstr);
        goto cleanup3;
    }
    
    ss->channel = g_io_channel_unix_new(fd);
    if (!ss->channel) {
        climpd_log_e(tag, "failed to create channel for '%s'\n", ss->path);
        goto cleanup3;
    }
    
    g_io_add_watch(ss->channel, G_IO_IN, &handle_socket, ss);
    g_io_channel_set_close_on_unref(ss->channel, true);
    
    ss->connection_handler = handler;
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;

cleanup3:
    close(fd);
cleanup2:
    free(ss->path);
cleanup1:
    clock_destroy(&ss->timer);
    return err;
}

void socket_server_destroy(struct socket_server *__restrict ss)
{
    g_io_channel_unref(ss->channel);
    
    unlink(ss->path);
    free(ss->path);
    
    clock_destroy(&ss->timer);
    
    climpd_log_i(tag, "destroyed\n");
}