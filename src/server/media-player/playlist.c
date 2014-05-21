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
#include <libvci/hash.h>
#include <libvci/macro.h>

#include "playlist.h"
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
    
    map_set_data_delete(&pl->map_path, (void(*)(void*)) &media_delete);
    
    err = random_init(&pl->rand);
    if(err < 0) {
        map_destroy(&pl->map_path);
        return err;
    }
    
    list_init(&pl->list);
    list_init(&pl->list_rand);
    list_init(&pl->list_done);
    
    pl->it = &pl->list;
    
    pl->rand_range = 0;
    
    pl->repeat  = false;
    pl->shuffle = false;
    
    return 0;
}

void playlist_destroy(struct playlist *__restrict pl)
{
    list_destroy(&pl->list_done, NULL);
    list_destroy(&pl->list_rand, NULL);
    list_destroy(&pl->list, NULL);
    random_destroy(&pl->rand);
    map_destroy(&pl->map_path);
}

void playlist_clear(struct playlist *__restrict pl)
{
    list_clear(&pl->list_done, NULL);
    list_clear(&pl->list_rand, NULL);
    list_clear(&pl->list, NULL);
    map_clear(&pl->map_path);
    
    pl->it = &pl->list;
    
    pl->rand_range = 0;
}

int playlist_insert_media(struct playlist *__restrict pl, struct media *m)
{
    int err;

    err = map_insert(&pl->map_path, m->path, m);
    if(err < 0)
        return err;
    
    list_insert_back(&pl->list, &m->link);
    list_insert_back(&pl->list_rand, &m->link_rand);
    
    pl->rand_range += 1;
    
    return 0;
}

void playlist_take_media(struct playlist *__restrict pl, struct media* m)
{
    m = map_take(&pl->map_path, m->path);

    if(pl->it == &m->link)
        pl->it = pl->it->prev;
    
    list_take(&m->link);
    list_take(&m->link_rand);
    
    pl->rand_range -= 1;
}

void playlist_delete_media(struct playlist *__restrict pl, struct media *m)
{
    playlist_take_media(pl, m);
    media_delete(m);
}

bool playlist_contains_media(const struct playlist *__restrict pl,
                             const struct media *m)
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
                          const char *__restrict path)
{
    struct media *m;
    
    m = map_take(&pl->map_path, path);
    if(!m)
        return;
    
    playlist_delete_media(pl, m);
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
    list_merge(&pl1->list_rand, &pl2->list_rand);
    list_merge(&pl1->list_done, &pl2->list_done);
    
    pl1->rand_range += pl2->rand_range;
    
    playlist_clear(pl2);
    
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

void playlist_set_shuffle(struct playlist *__restrict pl, bool shuffle)
{
    pl->shuffle = shuffle;
}

bool playlist_shuffle(const struct playlist *__restrict pl)
{
    return pl->shuffle;
}

void playlist_set_repeat(struct playlist *__restrict pl, bool repeat)
{
    pl->repeat = repeat;
}

bool playlist_repeat(const struct playlist *__restrict pl)
{
    return pl->repeat;
}

const struct media *playlist_next(struct playlist *__restrict pl)
{
    struct link *link;
    struct media *m;
    unsigned int index;
    
    if(playlist_empty(pl))
        return NULL;
    
    if(pl->shuffle) {
        if(list_empty(&pl->list_rand)) {
            list_merge(&pl->list_rand, &pl->list_done);
            
            pl->rand_range = map_size(&pl->map_path);
            
            if(!pl->repeat)
                return NULL;
        }
        
        pl->rand_range -= 1;
        
        index = random_uint_range(&pl->rand, 0, pl->rand_range);
        
        link = list_at(&pl->list_rand, index);
        
        list_take(link);
        list_insert_front(&pl->list_done, link);
        
        m = container_of(link, struct media, link_rand);
        
        pl->it = &m->link;
        
        return m;
    }

    pl->it = pl->it->next;
    
    if(pl->it == &pl->list) {
        if(!pl->repeat)
            return NULL;
        
        pl->it = pl->it->next;
    }
    
    return container_of(pl->it, struct media, link);
}

const struct media *playlist_previous(struct playlist *__restrict pl)
{
    struct media *m;
    struct link *link;
    
    if(pl->shuffle) {
        if(list_empty(&pl->list_done))
            return NULL;
        
        link = list_front(&pl->list_done);
        
        list_take(link);
        
        list_insert_back(&pl->list_done, link);
        
        m = container_of(link, struct media, link_rand);
        
        pl->it = &m->link;
        
        return m;
    }
    
    pl->it = pl->it->prev;
    
    if(pl->it == &pl->list) {
        if(!pl->repeat)
            return NULL;
        
        pl->it = pl->it->prev;
    }
    
    return container_of(pl->it, struct media, link);
}

const struct media *playlist_current(const struct playlist *__restrict pl)
{
    if(pl->it == &pl->list)
        return NULL;

    return container_of(pl->it, struct media, link);
}

int playlist_set_current(struct playlist *__restrict pl, struct media *m)
{
    int err;
    
    if(!playlist_contains_media(pl, m)) {
        err = map_insert(&pl->map_path, m->path, m);
        if(err < 0)
            return err;
        
        list_insert(pl->it, &m->link);
        list_insert_front(&pl->list_done, &m->link_rand);
    }
    
    pl->it = &m->link;
    
    return 0;
}

struct media *playlist_at(struct playlist *__restrict pl, unsigned int i)
{
    return container_of(list_at(&pl->list, i), struct media, link);
}

int playlist_index_of(const struct playlist *__restrict pl, 
                      const struct media *m)
{
    const struct link *link;
    int index;
    
    if(!playlist_contains_media(pl, m))
        return -1;
    
    link  = &m->link;
    index = 0;
    
    while(link != &pl->list) {
        link = link->prev;
        index += 1;
    }
    
    return index;
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