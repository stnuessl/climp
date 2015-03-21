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

#ifndef _MEDIA_LIST_H_
#define _MEDIA_LIST_H_

#include <stdbool.h>

#include <libvci/vector.h>

#include <obj/media.h>

struct media_list {
    struct vector media_vec;
};

int media_list_init(struct media_list *__restrict ml);

void media_list_destroy(struct media_list *__restrict ml);

int media_list_insert_back(struct media_list *__restrict ml, struct media *m);

int media_list_emplace_back(struct media_list *__restrict ml, const char *path);

int media_list_add_from_file(struct media_list *__restrict ml, 
                             const char *__restrict path);

struct media *media_list_at(struct media_list *__restrict ml, 
                            unsigned int index);

struct media *media_list_take(struct media_list *__restrict ml, 
                              unsigned int index);

struct media *media_list_take_back(struct media_list *__restrict ml);

unsigned int media_list_size(const struct media_list *__restrict ml);

bool media_list_empty(const struct media_list *__restrict ml);

#endif /* _MEDIA_LIST_H_ */