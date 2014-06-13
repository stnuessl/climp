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

#ifndef _MEDIA_SCHEDULER_H_
#define _MEDIA_SCHEDULER_H_

#include <libvci/vector.h>
#include <libvci/random.h>

#include "core/playlist.h"

struct media_scheduler {
    struct random random;
    
    struct playlist *playlist;
    struct vector *random_ready;
    struct vector *history;
    
    unsigned int running;
    
    bool repeat;
    bool shuffle;
};

struct media_scheduler *media_scheduler_new(struct playlist *pl);

void media_scheduler_delete(struct media_scheduler *__restrict ms);

int media_scheduler_init(struct media_scheduler *__restrict ms, 
                         struct playlist *pl);

void media_scheduler_destroy(struct media_scheduler *__restrict ms);

struct media *media_scheduler_next(struct media_scheduler *__restrict ms);

struct media *media_scheduler_previous(struct media_scheduler *__restrict ms);

struct media *media_scheduler_running(struct media_scheduler *__restrict ms);

int media_scheduler_set_running_media(struct media_scheduler *__restrict ms, 
                                      struct media *m);

int media_scheduler_set_running_track(struct media_scheduler *__restrict ms,
                                      unsigned int track);

int media_scheduler_insert_media(struct media_scheduler *__restrict ms,
                                 struct media *m);

struct media *media_scheduler_take_media(struct media_scheduler *__restrict ms,
                                         struct media *m);

int media_scheduler_set_playlist(struct media_scheduler *__restrict ms, 
                                 struct playlist *pl);

void media_scheduler_set_repeat(struct media_scheduler *__restrict ms, 
                                bool repeat);

bool media_scheduler_repeat(const struct media_scheduler *__restrict ms);

void media_scheduler_set_shuffle(struct media_scheduler *__restrict ms, 
                                 bool shuffle);

bool media_scheduler_shuffle(const struct media_scheduler *__restrict ms);

#endif /* _MEDIA_SCHEDULER_H_ */