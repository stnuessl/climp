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

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <gst/gst.h>

struct client {
    GIOChannel *io;
    
    uid_t uid;
    pid_t pid;
    
    int stderr;
    int stdout;
};

void client_init(struct client *__restrict client, pid_t pid, int unix_fd);

void client_destroy(struct client *__restrict client);

int client_unix_fd(const struct client *__restrict client);

void client_set_stdout(struct client *__restrict client, int fd);

void client_set_stderr(struct client *__restrict client, int fd);

void client_err(const struct client *__restrict client, const char *fmt, ...);

#endif /* _CLIENT_H_ */