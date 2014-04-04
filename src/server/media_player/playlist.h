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

#include <libvci/link.h>
#include <libvci/list.h>
#include <libvci/map.h>

#include "media.h"

struct playlist {
    struct map map_path;
    struct link list;
};

struct playlist *playlist_new(void);

void playlist_delete(struct playlist *__restrict pl);

int playlist_init(struct playlist *__restrict pl);

void playlist_destroy(struct playlist *__restrict pl);

void playlist_clear(struct playlist *__restrict pl);

int playlist_add_media(struct playlist *__restrict pl, struct media *m);

void playlist_take_media(struct playlist *__restrict pl, struct media *m);

void playlist_delete_media(struct playlist *__restrict pl, struct media *m);

bool playlist_contains_media(const struct playlist *__restrict pl,
                             struct media *m);

int playlist_add_media_path(struct playlist *__restrict pl, const char *path);

void playlist_delete_media_path(struct playlist *__restrict pl, const char *path);

struct media *playlist_first(struct playlist *__restrict pl);

struct media *playlist_last(struct playlist *__restrict pl);

struct media *playlist_next(struct playlist *__restrict pl, struct media *m);

struct media *playlist_previous(struct playlist *__restrict pl, 
                                struct media *m);

struct media *playlist_at(struct playlist *__restrict pl, unsigned int index);

int playlist_merge(struct playlist *__restrict pl1, struct playlist *pl2);

bool playlist_empty(const struct playlist *__restrict pl);

unsigned int playlist_size(const struct playlist *__restrict pl);


#define playlist_for_each(playlist, link)                                      \
    list_for_each(&(playlist)->list, (link))
    
#define playlist_for_each_safe(playlist, link, safe)                           \
    list_for_each_safe(&(playlist)->list, (link), (safe))

#endif /* _PLAYLIST_H_ */