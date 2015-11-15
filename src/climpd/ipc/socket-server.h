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

#ifndef _SOCKET_SERVER_H_
#define _SOCKET_SERVER_H_

#include <gst/gst.h>
#include <libvci/clock.h>

struct socket_server {
    struct clock timer;
    char *path;
    GIOChannel *channel;
    
    int (*connection_handler)(int fd);
};

int socket_server_init(struct socket_server *__restrict ss, 
                       const char *__restrict path,
                       int (*handler)(int fd));

void socket_server_destroy(struct socket_server *__restrict ss);


#endif /* _SOCKET_SERVER_H_ */