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

#include <stdlib.h>
#include <errno.h>

#include <libvci/random.h>
#include <libvci/vector.h>
#include <libvci/compare.h>

#include "core/media-scheduler.h"
#include "core/playlist.h"

#define MEDIA_SCHEDULER_MAX_HISTORY_SIZE 100

static void 
media_scheduler_reset_random_ready(struct media_scheduler *__restrict ms)
{
    unsigned int i, size;
    
    vector_clear(ms->random_ready);
    
    size = playlist_size(ms->playlist);
    
    for(i = 0; i < size; i++)
        vector_insert_back(ms->random_ready, (void *)(long) i);
}

struct media_scheduler *media_scheduler_new(struct playlist *pl)
{
    struct media_scheduler *ms;
    int err;
    
    ms = malloc(sizeof(*ms));
    if(!ms)
        return NULL;
    
    err = media_scheduler_init(ms, pl);
    if(err < 0) {
        free(ms);
        return NULL;
    }
    
    return ms;
}

void media_scheduler_delete(struct media_scheduler *__restrict ms)
{
    media_scheduler_destroy(ms);
    free(ms);
}

int media_scheduler_init(struct media_scheduler *__restrict ms, 
                         struct playlist *pl)
{
    unsigned int size;
    int err;
    
    err = random_init(&ms->random);
    if(err < 0)
        return -errno;
    
    size = playlist_size(pl);
    
    ms->random_ready = vector_new(size);
    if(!ms->random_ready) {
        err = -errno;
        goto cleanup1;
    }
    
    vector_set_data_compare(ms->random_ready, &compare_uint);
    
    ms->history = vector_new(MEDIA_SCHEDULER_MAX_HISTORY_SIZE);
    if(!ms->history) {
        err = -errno;
        goto cleanup2;
    }
    
    ms->playlist = pl;
    ms->running  = (unsigned int) -1;
    ms->repeat   = false;
    ms->shuffle  = false;
    
    media_scheduler_reset_random_ready(ms);
    
    return 0;
    
cleanup2:
    vector_delete(ms->random_ready);
cleanup1:
    random_destroy(&ms->random);
    
    return err;
}

void media_scheduler_destroy(struct media_scheduler *__restrict ms)
{
    vector_delete(ms->history);
    vector_delete(ms->random_ready);
    random_destroy(&ms->random);
}

struct media *media_scheduler_next(struct media_scheduler *__restrict ms)
{
    unsigned int size, r;
    int err;
    
    if(vector_size(ms->history) >= MEDIA_SCHEDULER_MAX_HISTORY_SIZE)
        vector_take_front(ms->history);
    
    if(ms->running != (unsigned int) -1) {
        err = vector_insert_back(ms->history, (void *)(long) ms->running);
        if(err < 0)
            return NULL;
    }
    
    if(ms->shuffle) {    
        if(vector_empty(ms->random_ready)) {
            media_scheduler_reset_random_ready(ms);
            
            if(!ms->repeat)
                return NULL;
        }
        
        size = vector_size(ms->random_ready);
        r = random_uint_range(&ms->random, 0, size - 1);
        
        ms->running = (unsigned int)(long) vector_take_at(ms->random_ready, r);
    } else {
        ms->running += 1;
        
        if(ms->running >= playlist_size(ms->playlist)) {
            if(!ms->repeat) {
                ms->running = (unsigned int) -1;
                return NULL;
            }
            
            ms->running = 0;
        }
    }
    
    return playlist_at(ms->playlist, ms->running);
}

struct media *media_scheduler_previous(struct media_scheduler *__restrict ms)
{
    if(vector_empty(ms->history))
        return NULL;
    
    ms->running = (unsigned int)(long) vector_take_back(ms->history);
    
    return playlist_at(ms->playlist, ms->running);
}

struct media *media_scheduler_running(struct media_scheduler *__restrict ms)
{
    return (ms->running != -1) ? playlist_at(ms->playlist, ms->running) : NULL;
}

int media_scheduler_set_running_media(struct media_scheduler *__restrict ms, 
                                struct media *m)
{
    unsigned int size;
    int index, err;
    
    size = playlist_size(ms->playlist);
    index = playlist_index_of(ms->playlist, m);
    
    if(index != (unsigned int) -1)
        return media_scheduler_set_running_track(ms, index);
    
    if(ms->running != (unsigned int) -1) {
        err = vector_insert_back(ms->history, (void *)(long) ms->running);
        if(err < 0)
            return err;
    }
    
    err = vector_insert_back(ms->random_ready, (void *)(long) size);
    if(err < 0)
        goto cleanup1;

    err = playlist_insert_at(ms->playlist, ms->running + 1, m);
    if(err < 0)
        goto cleanup2;
    
    ms->running += 1;
    
    if(ms->shuffle)
        vector_take_sorted(ms->random_ready, (void *)(long) index);
    
    ms->running = index;
    
    return 0;
    
cleanup2:
    vector_take_back(ms->random_ready);
cleanup1:
    vector_take_back(ms->history);
    return err;
}

int media_scheduler_set_running_track(struct media_scheduler *__restrict ms,
                                      unsigned int track)
{
    int err;
    
    if(track >= playlist_size(ms->playlist))
        return -EINVAL;
    
    if(ms->running != (unsigned int) - 1) {
        err = vector_insert_back(ms->history, (void *)(long) ms->running);
        if(err < 0)
            return err;
    }
    
    if(ms->shuffle)
        vector_take_sorted(ms->random_ready, (void *)(long) track);
    
    ms->running = track;
    
    return 0;
}

int media_scheduler_insert_media(struct media_scheduler *__restrict ms,
                                 struct media *m)
{
    unsigned int size;
    int err;
    
    err = playlist_insert_back(ms->playlist, m);
    if(err < 0)
        return err;
    
    size = vector_size(ms->random_ready);
    
    err = vector_insert_back(ms->random_ready, (void *)(long) size);
    if(err < 0) {
        playlist_take_back(ms->playlist);
        return err;
    }
    
    return 0;
}

struct media *media_scheduler_take_media(struct media_scheduler *__restrict ms,
                                         struct media *m)
{
    int index;
    
    index = playlist_index_of(ms->playlist, m);
    if(index == -1)
        return NULL;
    
    vector_take_sorted(ms->random_ready, (void *)(long) index);
    vector_take_all(ms->history, (void *)(long) index);
    
    return playlist_take_at(ms->playlist, index);
}

int media_scheduler_set_playlist(struct media_scheduler *__restrict ms, 
                                 struct playlist *pl)
{
    struct vector *ready;
    unsigned int i, size;
    int err;
    
    size = playlist_size(pl);
    
    if(size < playlist_size(ms->playlist)) {
        ms->playlist = pl;
        
        media_scheduler_reset_random_ready(ms);
        vector_clear(ms->history);
        
        return 0;
    }
    
    ready = vector_new(size);
    if(!ready)
        return -errno;
    
    vector_set_data_compare(ready, &compare_uint);
    
    for(i = 0; i < size; ++i) {
        err = vector_insert_back(ready, (void *)(long) i);
        if(err < 0)
            goto cleanup1;
    }

    vector_delete(ms->random_ready);
    
    ms->random_ready = ready;
    ms->running      = (unsigned int) -1;
        
    return 0;

cleanup1:
    vector_delete(ready);
    
    return err;
}

void media_scheduler_set_repeat(struct media_scheduler *__restrict ms, 
                                bool repeat)
{
    ms->repeat = repeat;
}

bool media_scheduler_repeat(const struct media_scheduler *__restrict ms)
{
    return ms->repeat;
}

void media_scheduler_set_shuffle(struct media_scheduler *__restrict ms, 
                                 bool shuffle)
{
    ms->shuffle = shuffle;
}

bool media_scheduler_shuffle(const struct media_scheduler *__restrict ms)
{
    return ms->shuffle;
}