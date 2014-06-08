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

#include <libvci/vector.h>
#include <libvci/macro.h>

#include "core/media.h"

struct playlist {
    struct vector vec_media;
    char *name;
};

struct playlist *playlist_new(const char *name);

void playlist_delete(struct playlist *__restrict pl);

int playlist_init(struct playlist* pl, const char* name);

void playlist_destroy(struct playlist *__restrict pl);

void playlist_clear(struct playlist *__restrict pl);

int playlist_insert_front(struct playlist *__restrict pl, struct media *m);

int playlist_insert_at(struct playlist *__restrict pl, 
                       unsigned int i, 
                       struct media *m);

int playlist_insert_back(struct playlist *__restrict pl, struct media *m);

void playlist_take_media(struct playlist *__restrict pl, struct media *m);

bool playlist_contains_media(const struct playlist *__restrict pl,
                             const struct media *m);

bool playlist_empty(const struct playlist *__restrict pl);

unsigned int playlist_size(const struct playlist *__restrict pl);

const char *playlist_name(const struct playlist *__restrict pl);

struct media *playlist_front(struct playlist *__restrict pl);

struct media *playlist_at(struct playlist *__restrict pl, unsigned int i);

struct media *playlist_back(struct playlist *__restrict pl);

struct media *playlist_take_front(struct playlist *__restrict pl);

struct media *playlist_take_at(struct playlist *__restrict pl, unsigned int i);

struct media *playlist_take_back(struct playlist *__restrict pl);

int playlist_index_of(struct playlist *__restrict pl, 
                      struct media *m);

#define playlist_for_each(playlist, data)                                      \
    vector_for_each(&(playlist)->vec_media, (data))


#endif /* _PLAYLIST_H_ */