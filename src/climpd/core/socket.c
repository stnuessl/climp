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
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <libvci/clock.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/socket.h>
#include <core/util.h>

#include "../../shared/ipc.h"

static const char *tag = "socket";

static GIOChannel *_io_chan;
static struct clock _timer;

static int (*_connection_handler)(int);

static gboolean handle_socket(GIOChannel *src, GIOCondition cond, void *data)
{
    struct ucred creds;
    socklen_t cred_len;
    int fd, err;
    
    (void) src;
    (void) cond;
    (void) data;
    
    clock_reset(&_timer);
    
    fd = accept(g_io_channel_unix_get_fd(_io_chan), NULL, NULL);
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
    
    err = _connection_handler(fd);
    if (err < 0) {
        climpd_log_e(tag, "handling connection on socket %d failed - %s\n",
                     fd, strerr(-err));
        goto out;
    }
    
out:
    close(fd);
    
    climpd_log_i(tag, "served connection on socket %d in %lu ms\n", fd, 
                 clock_elapsed_ms(&_timer));
    
    return true;
}

void socket_init(int (*func)(int))
{
    struct sockaddr_un addr;
    int fd, err;
    
    /* Setup Unix Domain Socket */
    err = unlink(IPC_SOCKET_PATH);
    if(err < 0 && errno != ENOENT) {
        climpd_log_e(tag, "failed to remove old server socket \"%s\" - %s\n",
                     IPC_SOCKET_PATH, strerr(errno));
        goto fail;
    }
    
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) {
        climpd_log_e(tag, "creating server socket failed - %s\n", errstr);
        goto fail;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        climpd_log_e(tag, "binding server socket failed - %s\n", errstr);
        goto fail;
    }
    
    err = listen(fd, 5);
    if(err < 0) {
        climpd_log_e(tag, "listening on server socket failed - %s\n", errstr);
        goto fail;
    }
    
    _io_chan = g_io_channel_unix_new(fd);
    if (!_io_chan) {
        climpd_log_e(tag, "initializing io channel for server socket failed\n");
        goto fail;
    }
    
    g_io_add_watch(_io_chan, G_IO_IN, &handle_socket, NULL);
    g_io_channel_set_close_on_unref(_io_chan, true);
    
    err = clock_init(&_timer, CLOCK_MONOTONIC);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize timer - %s\n", strerr(-err));
        goto fail;
    }
    
    clock_start(&_timer);
    
    _connection_handler = func;
    
    climpd_log_i(tag, "initialized\n");
    
    return;

fail:
    unlink(IPC_SOCKET_PATH);
    die_failed_init(tag);
}

void socket_destroy(void)
{
    clock_destroy(&_timer);
    g_io_channel_unref(_io_chan);
    unlink(IPC_SOCKET_PATH);
    
    climpd_log_i(tag, "destroyed\n");
}