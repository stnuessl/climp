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

#ifndef _MESSAGE_HANDLER_H_
#define _MESSAGE_HANDLER_H_

#include <libvci/clock.h>

#include "core/climpd-player.h"

#include "../../shared/ipc.h"

struct climpd_control {
    struct climpd_player player;
    struct message msg_in;
    struct message msg_out;
    struct clock cl;
    struct climpd_config *conf;
   
    GIOChannel *io_server;
    GIOChannel *io;
    GMainLoop *main_loop;
    
    int fd_socket;
    int fd_stdout;
    int fd_stderr;

    gboolean socket_handler_run;
    
};

struct climpd_control *climpd_control_new(int sock, struct climpd_config *conf);

void climpd_control_delete(struct climpd_control *__restrict cc);

int climpd_control_init(struct climpd_control *__restrict cc, 
                        int sock, struct climpd_config *conf);

void climpd_control_destroy(struct climpd_control *__restrict cc);

void climpd_control_run(struct climpd_control *__restrict cc);

#endif /* _MESSAGE_HANDLER_H_ */