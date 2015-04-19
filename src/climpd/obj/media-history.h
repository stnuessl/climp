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

#ifndef _MEDIA_HISTORY_H_
#define _MEDIA_HISTORY_H_

#include <libvci/link.h>

#include <obj/media.h>

struct media_history {
    struct link list;
    
    unsigned int size;
    unsigned int max_size;
};

void media_history_init(struct media_history *__restrict mh, 
                        unsigned int max_size);

void media_history_destroy(struct media_history *__restrict mh);

void media_history_insert(struct media_history *__restrict mh, 
                          struct media *m);

struct media *media_history_take(struct media_history *__restrict mh);

#endif /* _MEDIA_HISTORY_H_ */