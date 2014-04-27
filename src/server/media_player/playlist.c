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
#include "media.h"

static int media_compare(const void *__restrict s1, const void *__restrict s2)
{
    return strcmp(s1, s2);
}

static unsigned int media_hash(const void *__restrict path)
{
    const char *s;
    unsigned int hval;
    size_t len;
    
    s = path;
    hval = 1;
    len  = strlen(s);
    
    while(len--) {
        hval += *s++;
        hval += (hval << 10);
        hval ^= (hval >> 6);
        hval &= 0x0fffffff;
    }
    
    hval += (hval << 3);
    hval ^= (hval >> 11);
    hval += (hval << 15);
    
    return hval;
}

static struct media *playlist_active_list_at(struct playlist *__restrict pl,
                                             unsigned int index)
{
    struct link *link;
    
    list_for_each(&pl->list_next, link) {
        index -= 1;
        
        if(index == 0)
            return container_of(link, struct media, link);
    }
    
    return NULL;
}

static struct media *playlist_random_active_media(struct playlist *pl)
{
    unsigned int index, size;
    
    size = map_size(&pl->map_path);
    
    index = random_uint_range(&pl->rand, 0, size);
    
    return playlist_active_list_at(pl, index);
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
        
        err = playlist_add_media_path(pl, buf);
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
    
    err = map_init(&pl->map_path, 0, &media_compare, &media_hash);
    if(err < 0)
        return 0;
    
    err = random_init(&pl->rand);
    if(err < 0) {
        map_destroy(&pl->map_path);
        return err;
    }
    
    map_set_data_delete(&pl->map_path, (void(*)(void*)) &media_delete);
    
    list_init(&pl->list_all);
    list_init(&pl->list_next);
    list_init(&pl->list_prev);
    
    pl->shuffle = false;
    pl->repeat  = false;
    
    return 0;
}

void playlist_destroy(struct playlist *__restrict pl)
{
    list_destroy(&pl->list_prev, NULL);
    list_destroy(&pl->list_next, NULL);
    list_destroy(&pl->list_all, NULL);
    map_destroy(&pl->map_path);
}

void playlist_clear(struct playlist *__restrict pl)
{
    list_clear(&pl->list_prev, NULL);
    list_clear(&pl->list_next, NULL);
    list_clear(&pl->list_all, NULL);
    map_clear(&pl->map_path);
}

int playlist_add_media(struct playlist *__restrict pl, struct media *m)
{
    int err;
    
    assert(m && "Invalid argument");
    
    err = map_insert(&pl->map_path, m->path, m);
    if(err < 0)
        return err;
    
    list_insert_back(&pl->list_all, &m->link);
    list_insert_back(&pl->list_next, &m->link_state);
    
    return 0;
}

void playlist_take_media(struct playlist* pl, struct media* m)
{
    struct media *media;
    
    assert(m && "Invalid argument");
    
    media = map_take(&pl->map_path, m->path);
    
    assert(media == m && "Media mismatch");

    list_take(&media->link_state);
    list_take(&media->link);
}

void playlist_delete_media(struct playlist *__restrict pl, struct media *m)
{
    struct media *media;
    
    assert(m && "Invalid argument");
    
    media = map_take(&pl->map_path, m->path);
    
    assert(m == media && "Media mismatch");
    
    list_take(&media->link_state);
    list_take(&media->link);
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

int playlist_add_media_path(struct playlist *__restrict pl, const char *path)
{
    struct media *m;
    
    m = media_new(path);
    if(!m)
        return -errno;
    
    return playlist_add_media(pl, m);
}

void playlist_delete_media_path(struct playlist *__restrict pl, 
                                const char *path)
{
    struct media *m;
    
    assert(path && "Invalid argument");
    
    m = map_take(&pl->map_path, path);
    if(!m)
        return;
    
    list_take(&m->link_state);
    list_take(&m->link);
    
    media_delete(m);
}

struct media *playlist_begin(struct playlist *__restrict pl)
{
    struct link *link;
    struct media *m;
    
    if(map_empty(&pl->map_path))
        return NULL;
    
    list_merge(&pl->list_next, &pl->list_prev);
    
    if(pl->shuffle)
        m = playlist_random_active_media(pl);
    else
        m = container_of(list_front(&pl->list_next), struct media, link_state);
    
    list_take(&m->link_state);
    list_insert(&pl->list_prev, &m->link_state);
    
    return m;
}

struct media *playlist_next(struct playlist *__restrict pl, struct media *m)
{
    struct link *link;
    struct media *next;
    
    if(!map_contains(&pl->map_path, m->path))
        return NULL;

    list_take(&m->link_state);
    list_insert(&pl->list_prev);
    
    if(!list_empty(&pl->list_next)) {
        if(pl->shuffle)
            return playlist_random_active_media(pl);
        
        link = list_front(&pl->list_next);
        return container_of(link, struct media, link_state);
    }
    
    if(pl->repeat)
        return playlist_begin(pl);
    
    return NULL;
}

struct media *playlist_previous(struct playlist *__restrict pl, 
                                struct media *m)
{
    struct link *link;
    struct media *prev;
    
    if(!map_contains(&pl->map_path, m->path))
        return NULL;
    
    list_take(&m->link_state);
    list_insert_front(&pl->list_next, &m->link_state);
    
    if(list_empty(&pl->list_prev))
        return m;
    
    link = list_front(&pl->list_prev);
    
    return container_of(link, struct media, link_state);
}

struct media *playlist_at(struct playlist *__restrict pl, unsigned int index)
{
    struct link *link;
    
    playlist_for_each(pl, link) {
        index -= 1;
        
        if(index == 0)
            return container_of(link, struct media, link);
    }
    
    return NULL;
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