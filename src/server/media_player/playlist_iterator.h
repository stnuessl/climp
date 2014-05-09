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

#ifndef _PLAYLIST_ITERATOR_H_
#define _PLAYLIST_ITERATOR_H_

#include <stdbool.h>

#include <libvci/random.h>
#include <libvci/link.h>

#include "playlist.h"

struct playlist_iterator {
    struct link list;
    struct link *current;
    
    struct random rand;
    
    unsigned int size;
    bool shuffle;
    bool repeat;
};

struct playlist_iterator *playlist_iterator_new(void);

void playlist_iterator_delete(struct playlist_iterator *__restrict it);

int playlist_iterator_init(struct playlist_iterator *__restrict it);

void playlist_iterator_destroy(struct playlist_iterator *__restrict it);

void playlist_iterator_insert(struct playlist_iterator *__restrict it,
                              struct media *m);

void playlist_iterator_take(struct playlist_iterator *__restrict it, 
                            struct media *m);

bool playlist_iterator_has_next(const struct playlist_iterator *__restrict it);

const struct media *
playlist_iterator_next(struct playlist_iterator *__restrict it);

bool 
playlist_iterator_has_previous(const struct playlist_iterator *__restrict it);

const struct media *
playlist_iterator_previous(struct playlist_iterator *__restrict it);

void playlist_iterator_set_shuffle(struct playlist_iterator *__restrict it, 
                                   bool shuffle);

bool playlist_iterator_shuffle(const struct playlist_iterator *__restrict it);

void playlist_iterator_set_repeat(struct playlist_iterator *__restrict it, 
                                  bool repeat);

bool playlist_iterator_repeat(const struct playlist_iterator *__restrict it);


void playlist_iterator_set_current(struct playlist_iterator *__restrict it,
                                   struct media *m);

const struct media *
playlist_iterator_current(const struct playlist_iterator *__restrict it);

void playlist_iterator_do_shuffle(struct playlist_iterator *__restrict it);

#endif /* _PLAYLIST_ITERATOR_H_ */