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

#define MEDIA_FILE_PREFIX        "file://"
#define MEDIA_META_ELEMENT_SIZE  64


struct media *media_new(const char *path)
{
    struct media *media;
    
    if(path[0] != '/') {
        errno = EINVAL;
        return NULL;
    }

    media = malloc(sizeof(*media));
    if(!media)
        goto out;
    
    media->uri = malloc(sizeof(MEDIA_FILE_PREFIX) + strlen(path));
    if(!media->uri)
        goto cleanup1;
    
    media->uri = strcpy(media->uri, MEDIA_FILE_PREFIX);
    media->uri = strcat(media->uri, path);
    
    media->path = media->uri + sizeof(MEDIA_FILE_PREFIX) - 1;
    
    media->name = strrchr(media->path, '/');
    media->name = (media->name) ? media->name + 1 : media->path;
    
    strncpy(media->info.title, "Not parsed yet", MEDIA_META_ELEMENT_SIZE);
    strncpy(media->info.artist, "Not parsed yet", MEDIA_META_ELEMENT_SIZE);
    strncpy(media->info.album, "Not parsed yet", MEDIA_META_ELEMENT_SIZE);
    
    media->info.track = 0;
    media->info.duration = 0;
    media->info.seekable = false;
    
    return media;

cleanup1:
    free(media);
out:
    return NULL;
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
