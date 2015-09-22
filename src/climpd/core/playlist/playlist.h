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

#ifndef _PLAYLIST_H_
#define _PLAYLIST_H_

#include <libvci/vector.h>

#include <core/playlist/kfy.h>
#include <core/playlist/tag-reader.h>
#include <media/media.h>

struct playlist {
    struct vector vec_media;
    struct tag_reader tag_reader;
    struct kfy kfy;
    
    unsigned int index;
    bool repeat;
    bool shuffle;
};

int playlist_init(struct playlist *__restrict pl);

void playlist_destroy(struct playlist *__restrict pl);

void playlist_clear(struct playlist *__restrict pl);

int playlist_add_media(struct playlist *__restrict pl, struct media *m);

int playlist_add(struct playlist *__restrict pl, const char *__restrict path);

int playlist_load(struct playlist *__restrict pl, const char *__restrict path);

int playlist_save(struct playlist *__restrict pl, const char *__restrict path);

unsigned int playlist_index_of(struct playlist *__restrict pl, 
                               const char *__restrict path);

void playlist_remove(struct playlist *__restrict pl, int index);

unsigned int playlist_index(const struct playlist *__restrict pl);

void playlist_set_index(struct playlist *__restrict pl, int index);

struct media *playlist_at(struct playlist *__restrict pl, int index);

void playlist_set_shuffle(struct playlist *__restrict pl, bool shuffle);

bool playlist_shuffle(const struct playlist *__restrict pl);

void playlist_toggle_shuffle(struct playlist *__restrict pl);

void playlist_set_repeat(struct playlist *__restrict pl, bool repeat);

bool playlist_repeat(const struct playlist *__restrict pl);

void playlist_toggle_repeat(struct playlist *__restrict pl);

unsigned int playlist_next(struct playlist *__restrict pl);

unsigned int playlist_size(const struct playlist *__restrict pl);

bool playlist_empty(const struct playlist *__restrict pl);

void playlist_sort(struct playlist *__restrict pl);

#endif /* _PLAYLIST_H_ */