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

#include <libvci/link.h>

struct media_info {
    const char *title;
    const char *artist;
    const char *album;
};

struct media {
    struct media_info info;
    struct link link;
    
    char *uri;
    char *path;
    
    bool parsed;
};

struct media *media_new(const char *path);

void media_delete(struct media *__restrict media);

const struct media_info *media_info(const struct media *__restrict media);

bool media_is_parsed(const struct media *__restrict media);

#endif /* _MEDIA_H_ */