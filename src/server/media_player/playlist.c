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

#include <libvci/link.h>
#include <libvci/list.h>
#include <libvci/map.h>
#include <libvci/macro.h>

#include "playlist.h"
#include "playlist_iterator.h"
#include "media.h"

static int media_compare(const void *__restrict s1, const void *__restrict s2)
{
    return strcmp(s1, s2);
}

struct playlist *playlist_new(void)
{
    struct playlist *pl;
    int err;
    
    pl = malloc(sizeof(*pl));
    if(!pl)
        return NULL;
    
    err = playlist_init(pl);
    if(err < 0) {
        free(pl);
        return NULL;
    }
    
    return pl;
}

struct playlist *playlist_new_file(const char *__restrict path)
{
    struct playlist *pl;
    FILE *file;
    char *buf;
    size_t size;
    ssize_t n;
    int err;
    
    file = fopen(path, "r");
    if(!file)
        return NULL;
    
    pl = playlist_new();
    if(!pl) {
        fclose(file);
        return NULL;
    }
    
    buf  = NULL;
    size = 0;
    
    while(1) {
        n = getline(&buf, &size, file);
        if(n < 0)
            break;
        
        buf[n - 1] = '\0';
        
        if(buf[0] != '/')
            continue;
        
        err = playlist_insert_path(pl, buf);
        if(err < 0)
            continue;
    }
    
    free(buf);
    fclose(file);
    
    if(playlist_empty(pl)) {
        playlist_delete(pl);
        errno = EINVAL;
        return NULL;
    }
    
    return pl;
}

void playlist_delete(struct playlist *__restrict pl)
{
    playlist_destroy(pl);
    free(pl);
}

int playlist_init(struct playlist *__restrict pl)
{
    int err;
    
    err = map_init(&pl->map_path, 0, &media_compare, &hash_string);
    if(err < 0)
        return err;
    
    err = playlist_iterator_init(&pl->it);
    if(err < 0) {
        map_destroy(&pl->map_path);
        return err;
    }
    
    map_set_data_delete(&pl->map_path, (void(*)(void*)) &media_delete);
    
    list_init(&pl->list);
    
    return 0;
}

void playlist_destroy(struct playlist *__restrict pl)
{
    list_destroy(&pl->list, NULL);
    playlist_iterator_destroy(&pl->it);
    map_destroy(&pl->map_path);
}

void playlist_clear(struct playlist *__restrict pl)
{
    list_clear(&pl->list, NULL);
    map_clear(&pl->map_path);
}

int playlist_insert_media(struct playlist *__restrict pl, struct media *m)
{
    int err;
    
    assert(m && "Invalid argument");
    
    err = map_insert(&pl->map_path, m->path, m);
    if(err < 0)
        return err;
    
    list_insert_back(&pl->list, &m->link_pl);
    playlist_iterator_insert(&pl->it, m);
    
    return 0;
}

void playlist_take_media(struct playlist* pl, struct media* m)
{
    struct media *media;
    
    assert(m && "Invalid argument");
    
    media = map_take(&pl->map_path, m->path);
    
    assert(media == m && "Media mismatch");

    list_take(&media->link_it);
    playlist_iterator_take(&pl->it, media);
}

void playlist_delete_media(struct playlist *__restrict pl, struct media *m)
{
    struct media *media;
    
    assert(m && "Invalid argument");
    
    media = map_take(&pl->map_path, m->path);
    
    assert(m == media && "Media mismatch");
    
    list_take(&media->link_it);
    playlist_iterator_take(&pl->it, media);
    
    media_delete(media);
}

bool playlist_contains_media(const struct playlist *__restrict pl,
                             struct media *m)
{
    return map_contains(&pl->map_path, m->path);
}

bool playlist_contains(const struct playlist *__restrict pl, const char *path)
{
    return map_contains(&pl->map_path, path);
}

int playlist_insert_path(struct playlist *__restrict pl, 
                               const char *__restrict path)
{
    struct media *m;
    
    m = media_new(path);
    if(!m)
        return -errno;
    
    return playlist_insert_media(pl, m);
}

struct media *playlist_retrieve_path(struct playlist *__restrict pl, 
                                const char *__restrict path)
{
    return map_retrieve(&pl->map_path, path);
}

void playlist_delete_path(struct playlist *__restrict pl, 
                                const char *path)
{
    struct media *m;
    
    assert(path && "Invalid argument");
    
    m = map_take(&pl->map_path, path);
    if(!m)
        return;
    
    list_take(&m->link_it);
    playlist_iterator_take(&pl->it, media);
    
    media_delete(m);
}

int playlist_merge(struct playlist *__restrict pl1, struct playlist *pl2)
{
    struct link *link;
    struct media *m;
    int err;
    
    list_for_each(&pl2->list, link) {
        m = container_of(link, struct media, link);
        
        err = map_insert(&pl1->map_path, m->path, m);
        if(err < 0)
            goto out;
    }
    
    list_merge(&pl1->list, &pl2->list);
    list_merge(&pl1->it.list, &pl2->it.list);
    
    pl1->it.size = map_size(&pl1->map_path);
    
    return 0;

out:
    list_for_each(&pl2->list, link) {
        m = container_of(link, struct media, link);
        
        if(!map_take(&pl1->map_path, m->path))
            break;
    }
    
    return err;
}

bool playlist_empty(const struct playlist *__restrict pl)
{
    return map_empty(&pl->map_path);
}

unsigned int playlist_size(const struct playlist *__restrict pl)
{
    return map_size(&pl->map_path);
}

int playlist_save_to_file(const struct playlist *__restrict pl, 
                          const char *__restrict path,
                          const char *__restrict mode)
{
    FILE *file;
    const struct link *link;
    const struct media *m;
    
    file = fopen(path, mode);
    if(!file)
        return -errno;
    
    playlist_for_each(pl, link) {
        m = container_of(link, struct media, link);
        
        fprintf(file, "%s\n", m->path);
    }
    
    fclose(file);
    
    return 0;
}