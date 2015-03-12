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

#include <libvci/vector.h>

#include <core/media.h>
#include <core/media-list.h>
#include <core/kfy.h>

struct playlist {
    struct vector vec_media;
    struct kfy kfy;
    
    unsigned int index;
    bool repeat;
    bool shuffle;
};

int playlist_init(struct playlist *__restrict pl, bool repeat, bool shuffle);

void playlist_destroy(struct playlist *__restrict pl);

void playlist_clear(struct playlist *__restrict pl);

int playlist_insert_back(struct playlist *__restrict pl, struct media *m);

int playlist_emplace_back(struct playlist *__restrict pl, const char *path);

int playlist_add_media_list(struct playlist *__restrict pl, 
                            struct media_list *__restrict ml);

struct media *playlist_at(struct playlist *__restrict pl, int index);

struct media *playlist_take(struct playlist *__restrict pl, int index);

void playlist_update_index(struct playlist *__restrict pl, int index);

unsigned int playlist_index_of(const struct playlist *__restrict pl,
                               const struct media *__restrict m);

unsigned int playlist_index_of_path(struct playlist *__restrict pl,
                                    const char *__restrict path);

unsigned int playlist_size(const struct playlist *__restrict pl);

bool playlist_empty(const struct playlist *__restrict pl);

struct media *playlist_next(struct playlist *__restrict pl);

bool playlist_toggle_shuffle(struct playlist *__restrict pl);

bool playlist_toggle_repeat(struct playlist *__restrict pl);

void playlist_set_shuffle(struct playlist *__restrict pl, bool shuffle);

void playlist_set_repeat(struct playlist *__restrict pl, bool repeat);

bool playlist_shuffle(const struct playlist *__restrict pl);

bool playlist_repeat(const struct playlist *__restrict pl);

#endif /* _PLAYLIST_H_ */