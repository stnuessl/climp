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
#include <libvci/error.h>

#include "util/climpd-log.h"
#include "core/playlist.h"
#include "core/media.h"

static const char *tag = "playlist";

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

struct playlist *playlist_new_from_file(const char *__restrict path)
{
    struct playlist *pl;
    int err;
    
    pl = playlist_new(path);
    if(!pl)
        return NULL;
    
    err = playlist_load_from_file(pl, path);
    if(err < 0) {
        climpd_log_e(tag, "loading playlist \"%s\" failed - %s\n", path, 
                     strerr(-err));
        playlist_delete(pl);
        return NULL;
    }
    
    climpd_log_i(tag, "loaded playlist \"%s\"\n", path);
    
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
    
    pl->path = strdup(name);
    if(!pl->path)
        return -errno;
    
    err = vector_init(&pl->vec_media, 32);
    if(err < 0) {
        free(pl->path);
        return err;
    }
    
    vector_set_data_delete(&pl->vec_media, (void (*)(void *)) &media_delete);
    
    return 0;
}

void playlist_destroy(struct playlist *__restrict pl)
{
    vector_destroy(&pl->vec_media);
    free(pl->path);
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
    return vector_contains(&pl->vec_media, m);
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
    return pl->path;
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

unsigned int playlist_index_of(struct playlist *__restrict pl, 
                               struct media *m)
{
    return vector_index_of(&pl->vec_media, m);
}

int playlist_load_from_file(struct playlist *__restrict pl, 
                            const char *__restrict path)
{
    struct media *m;
    FILE *file;
    char *line;
    size_t size;
    ssize_t n;
    int err;
    
    file = fopen(path, "r");
    if(!file)
        return -errno;
    
    line  = NULL;
    size = 0;
    
    while(1) {
        n = getline(&line, &size, file);
        if(n < 0)
            break;
        
        if(n == 0)
            continue;
        
        if(line[0] == '#' || line[0] == ';')
            continue;
        
        line[n - 1] = '\0';
        
        if(line[0] != '/') {
            climpd_log_w(tag, "skipping %s - no absolute path\n", line);
            continue;
        }
        
        m = media_new(line);
        if(!m) {
            err = -errno;
            climpd_log_w(tag, "creating media object for %s failed - %s\n",
                        line, strerr(-err));
            continue;
        }
        
        err = playlist_insert_back(pl, m);
        if(err < 0) {
            climpd_log_w(tag, "adding %s to playlist failed - %s\n", line,
                         strerr(-err));
            media_delete(m);
            continue;
        }
    }
    
    free(line);
    fclose(file);
    
    if(playlist_empty(pl)) {
        climpd_log_w(tag, "playlist \"%s\" has no valid entries\n", path);
        return -ENOENT;
    }
    
    return 0;
}