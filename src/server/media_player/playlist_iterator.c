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

#include "playlist_iterator.h"

#include <libvci/random.h>
#include <libvci/macro.h>

#include "playlist.h"
#include "media.h"

struct playlist_iterator *playlist_iterator_new(void)
{
    struct playlist_iterator *it;
    int err;
    
    it = malloc(sizeof(*it));
    if(!it)
        return NULL;
    
    err = playlist_iterator_init(it);
    if(err < 0) {
        free(it);
        return NULL;
    }
    
    return it;
}

void playlist_iterator_delete(struct playlist_iterator *__restrict it)
{
    playlist_iterator_destroy(it);
    free(it);
}

int playlist_iterator_init(struct playlist_iterator *__restrict it)
{
    int err;
    
    err = random_init(&it->rand);
    if(err < 0)
        return err;
    
    list_init(&it->list);
    
    it->current = &it->list;
    
    it->size    = 0;
    it->shuffle = false;
    it->repeat  = false;
    
    return 0;
}

void playlist_iterator_destroy(struct playlist_iterator *__restrict it)
{
    random_destroy(&it->rand);
}

void playlist_iterator_insert(struct playlist_iterator *__restrict it,
                              struct media *m)
{
    list_insert_back(&it->list, &m->link_it);
    it->size += 1;
}

void playlist_iterator_take(struct playlist_iterator *__restrict it, 
                            struct media *m)
{
    if(it->current == &m->link_it)
        it->current = it->current->prev;
        
    list_take(&m->link_it);
    it->size += 1;
}

bool playlist_iterator_has_next(const struct playlist_iterator *__restrict it)
{
    if(it->repeat)
        return true;
    
    /* Start position */
    if(it->current == &it->list)
        return true;
    
    return it->current->next != &it->list;
}

const struct media *
playlist_iterator_next(struct playlist_iterator *__restrict it)
{
    it->current = it->current->next;
    
    if(it->current != &it->list)
        return container_of(it->current, struct media, link_it);
    
    if(!it->repeat)
        return NULL;
    
    if(it->shuffle)
        playlist_iterator_shuffle(it);
    
    it->current = list_front(&it->list);
    
    return container_of(it->current, struct media, link_it);
}

bool 
playlist_iterator_has_previous(const struct playlist_iterator *__restrict it)
{
    if(it->repeat)
        return true;
    
    if(it->current == &it->list)
        return true;
    
    return it->current->prev != &it->list;
}

const struct media *
playlist_iterator_previous(struct playlist_iterator *__restrict it)
{
    it->current = it->current->prev;
    
    if(it->current != &it->list)
        return container_of(it->current, struct media, link_it);
    
    if(!it->repeat)
        return NULL;
    
    it->current = list_back(&it->list);
    
    return container_of(it->current, struct media, link_it);
}

void playlist_iterator_set_shuffle(struct playlist_iterator *__restrict it, 
                                   bool shuffle)
{
    /* TODO: How to go back to default order? How to switch into random? */
    it->shuffle = shuffle;
}

bool playlist_iterator_shuffle(const struct playlist_iterator *__restrict it)
{
    return it->shuffle;
}

void playlist_iterator_set_repeat(struct playlist_iterator *__restrict it, 
                                  bool repeat)
{
    it->repeat = repeat;
}

bool playlist_iterator_repeat(const struct playlist_iterator *__restrict it)
{
    return it->repeat;
}

void playlist_iterator_set_current(struct playlist_iterator *__restrict it,
                                   struct media *m)
{
    if(it->shuffle) {
        playlist_iterator_do_shuffle(it);
        list_take(&m->link_it);
        list_insert_front(&it->list, &m->link_it);
    }
    
    it->current = &m->link_it;
}

const struct media *
playlist_iterator_current(const struct playlist_iterator *__restrict it)
{
    return container_of(it->current, struct media, link_it);
}

void playlist_iterator_do_shuffle(struct playlist_iterator *__restrict it)
{
    struct link list, *link;
    unsigned int index;
    
    /* Create new list with random order */
    list_init(&list);
    
    while(!list_empty(&it->list)) {
        index = random_uint_range(&it->rand, 0, it->size - 1);
        
        link = list_at(&it->list, index);
        
        list_take(link);
        list_insert_back(&list, link);
    }
    
    /* Merge it back */
    list_merge(&it->list, &list);
}

