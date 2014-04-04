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

#include "../shared/ipc.h"

struct climpd {
    struct message msg;
    int fd;

    unsigned int int_err_cnt;
    unsigned int ext_err_cnt;
};

struct climpd *climpd_new(void);

void climpd_delete(struct climpd *__restrict cc);

void climpd_get_playlist(struct climpd *__restrict c);

void climpd_get_files(struct climpd *__restrict c);

void climpd_get_volume(struct climpd *__restrict c);

void climpd_get_status(struct climpd *__restrict c);

void climpd_set_status(struct climpd *__restrict c, const char *arg);

void climpd_set_playlist(struct climpd *__restrict c, const char *arg);

void climpd_set_volume(struct climpd *__restrict c, const char *arg);

void climpd_play_next(struct climpd *__restrict c);

void climpd_play_previous(struct climpd *__restrict c);

void climpd_play_file(struct climpd *__restrict c, const char *arg);

void climpd_play_track(struct climpd *__restrict c, const char *arg);

void climpd_add_media(struct climpd *__restrict c, const char *arg);

void climpd_remove_media(struct climpd *__restrict c, const char *arg);

#endif /* _CLIENT_H_ */