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

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include <gst/gst.h>

#include "client.h"

void client_init(struct client *__restrict client, pid_t pid, int unix_fd)
{
    client->pid     = pid;
    client->io      = g_io_channel_unix_new(unix_fd);
    client->stdout  = -1;
    client->stderr  = -1;
}

void client_destroy(struct client *__restrict client)
{
    g_io_channel_unref(client->io);
    
    if(client->stdout >= 0)
        close(client->stdout);
    
    if(client->stderr >= 0)
        close(client->stderr);
}

int client_unix_fd(const struct client *__restrict client)
{
    return g_io_channel_unix_get_fd(client->io);
}

void client_set_stdout(struct client *__restrict client, int fd)
{
    client->stdout = fd;
}

void client_set_stderr(struct client *__restrict client, int fd)
{
    client->stderr = fd;
}

void client_err(const struct client *__restrict client, const char *fmt, ...)
{
    va_list args;
    
    if(client->stderr < 0)
        return;
    
    va_start(args, fmt);
    
    vdprintf(client->stderr, fmt, args);
    
    va_end(args);
}