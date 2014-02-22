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
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <libvci/link.h>
#include <libvci/list.h>
#include <libvci/map.h>
#include <libvci/macro.h>

#include "playlist.h"
#include "media.h"


#define MEDIA_NOT_IN_PLAYLIST "Media not in playlist"

static int media_compare(const void *__restrict s1, 
                         const void *__restrict s2)
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

void playlist_delete(struct playlist *__restrict pl)
{
    playlist_destroy(pl);
    free(pl);
}

int playlist_init(struct playlist *__restrict pl)
{
    int err;
    
    err = map_init(&pl->map, 0, &media_compare, &media_hash);
    if(err < 0)
        return 0;
    
    map_set_data_delete(&pl->map, (void(*)(void*)) &media_delete);
    
    list_init(&pl->list);
    
    return 0;
}

void playlist_destroy(struct playlist *__restrict pl)
{
    list_destroy(&pl->list, NULL);
    map_destroy(&pl->map);
}

void playlist_clear(struct playlist *__restrict pl)
{
    list_clear(&pl->list, NULL);
    map_clear(&pl->map);
}

int playlist_add_media(struct playlist *__restrict pl, struct media *m)
{
    int err;
        
    err = map_insert(&pl->map, m->path, m);
    if(err < 0)
        return err;
    
    list_insert_back(&pl->list, &m->link);
    
    return 0;
}

void playlist_remove_media(struct playlist* pl, struct media* m)
{
    assert(map_contains(&pl->map, m->path) && MEDIA_NOT_IN_PLAYLIST);
    
    if(map_take(&pl->map, m->path))
        list_take(&m->link);
}

void playlist_delete_media(struct playlist *__restrict pl, struct media *m)
{
    m = map_take(&pl->map, m->path);
    
    assert(m && MEDIA_NOT_IN_PLAYLIST);
    
    list_take(&m->link);
    media_delete(m);
}

bool playlist_contains_media(const struct playlist *__restrict pl,
                             struct media *m)
{
    return map_contains(&pl->map, m->path);
}

int playlist_add_file(struct playlist *__restrict pl, const char *path)
{
    struct media *m;
    
    m = media_new(path);
    if(!m)
        return -errno;
    
    return playlist_add_media(pl, m);
}

void playlist_remove_file(struct playlist *__restrict pl, const char* path)
{
    struct media *m;
    
    m = map_take(&pl->map, path);
    if(!m)
        return;
    
    media_delete(m);
}

struct media *playlist_first(struct playlist *__restrict pl)
{
    return container_of(list_front(&pl->list), struct media, link);
}

struct media *playlist_last(struct playlist *__restrict pl)
{
    return container_of(list_back(&pl->list), struct media, link);
}

struct media *playlist_next(struct playlist *__restrict pl, struct media *m)
{
    assert(map_contains(&pl->map, m->path) && MEDIA_NOT_IN_PLAYLIST);
    
    if(unlikely(&m->link == list_back(&pl->list)))
        return NULL;
    
    return container_of(m->link.next, struct media, link);
}

struct media *playlist_previous(struct playlist *__restrict pl, 
                                struct media *m)
{
    assert(map_contains(&pl->map, m->path) && MEDIA_NOT_IN_PLAYLIST);
    
    if(unlikely(&m->link == list_front(&pl->list)))
        return NULL;
    
    return container_of(m->link.prev, struct media, link);
}

int playlist_merge(struct playlist *__restrict pl1, struct playlist *pl2)
{
    struct link *link;
    struct media *m;
    int err;
    
    list_for_each(&pl2->list, link) {
        m = container_of(link, struct media, link);
        
        err = map_insert(&pl1->map, m->path, m);
        if(err < 0)
            goto out;
    }
    
    list_merge(&pl1->list, &pl2->list);
    
    return 0;

out:
    list_for_each(&pl2->list, link) {
        m = container_of(link, struct media, link);
        
        if(!map_take(&pl1->map, m->path))
            break;
    }
    
    return err;
}