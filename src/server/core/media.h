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

#ifndef _MEDIA_H_
#define _MEDIA_H_

#include <stdbool.h>
#include <stdatomic.h>

#include <libvci/link.h>

#define MEDIA_META_ELEMENT_SIZE  64

struct media_info {
    char title[MEDIA_META_ELEMENT_SIZE];
    char artist[MEDIA_META_ELEMENT_SIZE];
    char album[MEDIA_META_ELEMENT_SIZE];
    unsigned int track; 
    unsigned int duration;
    bool seekable;
};

struct media {
    struct media_info info;
    
    char *uri;
    char *path;
    char *name;
    
    bool parsed;
    
    atomic_int ref_count;
};

struct media *media_new(const char *__restrict path);

struct media *media_ref(struct media *__restrict media);

void media_unref(struct media *__restrict media);

struct media_info *media_info(struct media *__restrict media);

const char *media_uri(const struct media *__restrict media);

const char *media_path(const struct media *__restrict media);

const char *media_name(const struct media *__restrict media);

void media_set_parsed(struct media *__restrict media);

bool media_is_parsed(const struct media *__restrict media);

int media_compare(const struct media *__restrict m1, 
                  const struct media *__restrict m2);

int media_path_compare(const struct media *__restrict media, 
                       const char *__restrict path);

#endif /* _MEDIA_H_ */