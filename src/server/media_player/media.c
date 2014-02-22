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

#include "media.h"

#define MEDIA_FILE_PREFIX "file://"

static struct media *media_new_abs_path(const char *path)
{
    struct media *media;
    
    media = malloc(sizeof(*media));
    if(!media)
        return NULL;
    
    media->uri = malloc(sizeof(MEDIA_FILE_PREFIX) + strlen(path));
    if(!media->uri) {
        free(media);
        return NULL;
    }
    
    media->uri = strcpy(media->uri, MEDIA_FILE_PREFIX);
    media->uri = strcat(media->uri, path);
    
    media->path = media->uri + sizeof(MEDIA_FILE_PREFIX) - 1;
    
    media->parsed = false;
    
    media->info.title  = NULL;
    media->info.artist = NULL;
    media->info.album  = NULL;
    
    return media;
}

struct media *media_new(const char *path)
{
    struct media *media;
    char *abs_path;
    
    if(path[0] == '/') 
        return media_new_abs_path(path);
    
    abs_path = realpath(path, NULL);

    media = media_new_abs_path(abs_path);
    
    free(abs_path);
    return media;
}

void media_delete(struct media *__restrict media)
{
    free(media->uri);
    free(media);
}

const struct media_info *media_info(const struct media *__restrict media)
{
    return &media->info;
}

bool media_is_parsed(const struct media *__restrict media)
{
    return media->parsed;
}