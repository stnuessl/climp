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
#include <libvci/options.h>

#include "core/climpd-player.h"

#include "../../shared/ipc.h"

struct climpd_control {
    struct climpd_player player;
    struct clock cl;
    struct climpd_config *conf;
   
    GIOChannel *io_server;
    GMainLoop *main_loop;
    
    struct vector add_args;
    struct vector play_args;
    struct vector list_args;
    struct vector volume_args;
    
    bool pause;
    bool quit;
    
    struct program_options po[] = {
        { "add", "a",     PO_STRING_VEC, &add_args,    "" },
        { "play", "p",    PO_STRING,     &play_arg,    "" },
        { "pause", "",    PO_BOOL,       &pause,       "" },
        { "list", "l",    PO_STRING_VEC, &list_args,   "" },
        { "remove", "",   PO_STRING_VEC, &remove_args, "" },
        { "repeat", "r",  PO_STRING,     &repeat_arg,  "" },
        { "shuffle", "s", PO_STRING      &shuffle_arg, "" },
        { "volume", "v",  PO_STRING,     &volume_arg,  "" },
        { "quit", "q",    PO_BOOL,       &quit,        "" },
        { "previous", "", PO_BOOL,       &previous,    "" },  
    };
    
    struct options opts;
};

struct climpd_control *climpd_control_new(int sock,
                                          const char *__restrict config_path);

void climpd_control_delete(struct climpd_control *__restrict cc);

int climpd_control_init(struct climpd_control *__restrict cc, 
                        int sock,
                        const char *__restrict config_path);

void climpd_control_destroy(struct climpd_control *__restrict cc);

void climpd_control_run(struct climpd_control *__restrict cc);

void climpd_control_quit(struct climpd_control *__restrict cc);

int climpd_control_reload_config(struct climpd_control *__restrict cc);

#endif /* _MESSAGE_HANDLER_H_ */