/*
 * Copyright (C) 2015  Steffen Nüssle
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

#include <libvci/list.h>
#include <libvci/macro.h>

#include <obj/media-history.h>

static void media_destroy_from_link(struct link *link)
{
    media_unref(container_of(link, struct media, h_link));
}

void media_history_init(struct media_history *__restrict mh, 
                        unsigned int max_size)
{
    list_init(&mh->list);
    mh->size = 0;
    mh->max_size = max_size;
}

void media_history_destroy(struct media_history *__restrict mh)
{
    list_destroy(&mh->list, &media_destroy_from_link);
}

void media_history_insert(struct media_history *__restrict mh,
                          struct media *m)
{
    ++mh->size;
    
    while (mh->size-- > mh->max_size)
        media_destroy_from_link(list_take_back(&mh->list));
    
    media_ref(m);
    list_insert_front(&mh->list, &m->h_link);
}

struct media *media_history_take(struct media_history *__restrict mh)
{
    --mh->size;
    
    return container_of(list_take_front(&mh->list), struct media, h_link);
}