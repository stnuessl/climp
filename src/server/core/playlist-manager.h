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

#ifndef _PLAYLIST_MANAGER_H_
#define _PLAYLIST_MANAGER_H_

#include "playlist.h"

#define CLIMPD_DEFAULT_PLAYLIST "climpd default playlist"

int playlist_manager_init(void);

void playlist_manager_destroy(void);

int playlist_manager_load_from_file(const char *__restrict path);

int playlist_manager_save_to_file(const char *__restrict path);

int playlist_manager_insert(struct playlist *__restrict pl);

struct playlist *playlist_manager_retrieve(const char *__restrict name);

struct playlist *playlist_manager_take(const char *__restrict name);

void playlist_manager_delete_playlist(const char *__restrict name);

void playlist_manager_print(int fd);

#endif /* _PLAYLIST_MANAGER_H_ */