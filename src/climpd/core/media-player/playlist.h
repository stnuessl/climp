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

#ifndef _PLAYLIST_H_
#define _PLAYLIST_H_

#include <stdbool.h>

#include <obj/media.h>
#include <obj/media-list.h>

void playlist_init(bool repeat, bool shuffle);

void playlist_destroy(void);

void playlist_clear(void);

int playlist_insert_back(struct media *m);

int playlist_emplace_back(const char *path);

int playlist_set_media_list(struct media_list *__restrict ml);

int playlist_add_media_list(struct media_list *__restrict ml);

void playlist_remove_media_list(struct media_list *__restrict ml);

struct media *playlist_at(int index);

struct media *playlist_take(int index);

void playlist_update_index(int index);

unsigned int playlist_index(void);

unsigned int playlist_index_of(const struct media *__restrict m);

unsigned int playlist_index_of_path(const char *__restrict path);

unsigned int playlist_size(void);

bool playlist_empty(void);

void playlist_sort(void);

struct media *playlist_next(void);

bool playlist_toggle_shuffle(void);

bool playlist_toggle_repeat(void);

void playlist_set_shuffle(bool shuffle);

bool playlist_shuffle(void);

bool playlist_repeat(void);

void playlist_save(const char *__restrict path);

void playlist_load(const char *__restrict path);


#endif /* _PLAYLIST_H_ */