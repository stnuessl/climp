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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <libvci/vector.h>
#include <libvci/macro.h>

#include "core/playlist.h"
#include "core/media.h"

struct playlist *playlist_new(const char *__restrict name)
{
    struct playlist *pl;
    int err;
    
    pl = malloc(sizeof(*pl));
    if(!pl)
        return NULL;
    
    err = playlist_init(pl, name);
    if(err < 0) {
        free(pl);
        return NULL;
    }
    
    return pl;
}

void playlist_delete(struct playlist *__restrict pl)
{
    playlist_destroy(pl);
    free(pl);
}

int playlist_init(struct playlist *__restrict pl, const char *__restrict name)
{
    int err;
    
    pl->name = strdup(name);
    if(!pl->name)
        return -errno;
    
    err = vector_init(&pl->vec_media, 32);
    if(err < 0) {
        free(pl->name);
        return err;
    }
    
    return 0;
}

void playlist_destroy(struct playlist *__restrict pl)
{
    vector_destroy(&pl->vec_media);
    free(pl->name);
}

void playlist_clear(struct playlist *__restrict pl)
{
    vector_clear(&pl->vec_media); 
}

int playlist_insert_front(struct playlist *__restrict pl, struct media *m)
{
    return vector_insert_front(&pl->vec_media, m);
}

int playlist_insert_at(struct playlist *__restrict pl, 
                       unsigned int i, 
                       struct media *m)
{
    assert(i <= playlist_size(pl) && "Invalid playlist index");
    
    return vector_insert_at(&pl->vec_media, i, m);
}

int playlist_insert_back(struct playlist *__restrict pl, struct media *m)
{
    return vector_insert_back(&pl->vec_media, m);
}


void playlist_take_media(struct playlist *__restrict pl, struct media* m)
{
    vector_take(&pl->vec_media, m);
}

bool playlist_contains_media(const struct playlist *__restrict pl,
                             const struct media *m)
{
    struct media **media;
    
    playlist_for_each(pl, media) {
        if(*media == m)
            return true;
    }
    
    return false;
}

bool playlist_empty(const struct playlist *__restrict pl)
{
    return vector_empty(&pl->vec_media);
}

unsigned int playlist_size(const struct playlist *__restrict pl)
{
    return vector_size(&pl->vec_media);
}

const char *playlist_name(const struct playlist *__restrict pl)
{
    return pl->name;
}

struct media *playlist_front(struct playlist *__restrict pl)
{
    return *vector_front(&pl->vec_media);
}

struct media *playlist_at(struct playlist *__restrict pl, unsigned int i)
{
    assert(i < playlist_size(pl) && "Invalid playlist index");
    
    return *vector_at(&pl->vec_media, i);
}

struct media *playlist_back(struct playlist *__restrict pl)
{
    return *vector_back(&pl->vec_media);
}

struct media *playlist_take_front(struct playlist *__restrict pl)
{
    return vector_take_front(&pl->vec_media);
}

struct media *playlist_take_at(struct playlist *__restrict pl, unsigned int i)
{
    assert(i < playlist_size(pl) && "Invalid playlist index");
    
    return vector_take_at(&pl->vec_media, i);
}

struct media *playlist_take_back(struct playlist *__restrict pl)
{
    return vector_take_back(&pl->vec_media);
}

int playlist_index_of(struct playlist *__restrict pl, 
                      struct media *m)
{
    unsigned int size;
    int i;
    
    size = vector_size(&pl->vec_media);
    
    for(i = 0; i < size; ++i) {
        if(*vector_at(&pl->vec_media, i) == m)
            return i;
    }
    
    return -1;
}