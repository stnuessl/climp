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

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "../shared/ipc.h"

struct climpd_control {
    struct message msg;
    int fd;
};

struct climpd_control *climpd_control_new(void);

void climpd_control_delete(struct climpd_control *__restrict cc);

int climpd_control_play(struct climpd_control *__restrict cc, 
                        const char *arg);

int climpd_control_stop(struct climpd_control *__restrict cc);

int climpd_control_set_volume(struct climpd_control *__restrict cc, 
                              const char *arg);

int climpd_control_toggle_mute(struct climpd_control *__restrict cc);

int climpd_control_shutdown(struct climpd_control *__restrict cc);

int climpd_control_add(struct climpd_control *__restrict cc, const char *arg);

int climpd_control_next(struct climpd_control *__restrict cc);

int climpd_control_previous(struct climpd_control *__restrict cc);

#endif /* _CLIENT_H_ */