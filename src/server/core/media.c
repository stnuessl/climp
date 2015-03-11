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
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include <libvci/filesystem.h>

#include "media.h"

#define MEDIA_FILE_PREFIX        "file://"
#define MEDIA_META_ELEMENT_SIZE  64

struct media *media_new(const char *__restrict path)
{
    struct media *media;
    int err;
    
    if (!path_is_absolute(path)) {
        char *rpath = realpath(path, NULL);
        if (!rpath)
            return NULL;
        
        media = media_new(rpath);
        
        err = errno;
        free(rpath);
        errno = err;
        
        return media;
    }
    
    if (!path_is_reg(path)) {
        errno = EINVAL;
        return NULL;
    }

    media = malloc(sizeof(*media));
    if(!media)
        return NULL;
    
    media->uri = malloc(sizeof(MEDIA_FILE_PREFIX) + strlen(path));
    if(!media->uri)
        goto cleanup1;
    
    strncpy(media->info.title, "", MEDIA_META_ELEMENT_SIZE);
    strncpy(media->info.artist, "", MEDIA_META_ELEMENT_SIZE);
    strncpy(media->info.album, "", MEDIA_META_ELEMENT_SIZE);
    
    media->info.track = 0;
    media->info.duration = 0;
    media->info.seekable = false;
    
    media->uri = strcpy(media->uri, MEDIA_FILE_PREFIX);
    media->uri = strcat(media->uri, path);
    
    media->path = media->uri + sizeof(MEDIA_FILE_PREFIX) - 1;
    
    media->name = strrchr(media->path, '/');
    media->name = (media->name) ? media->name + 1 : media->path;
    
    media->parsed = false;
    
    atomic_init(&media->ref_count, 1);

    return media;

cleanup1:
    free(media);
    
    return NULL;
}

struct media *media_ref(struct media *__restrict media)
{
    atomic_fetch_add(&media->ref_count, 1);
    return media;
}

void media_unref(struct media *__restrict media)
{
    if (atomic_fetch_sub(&media->ref_count, 1) == 1) {
        free(media->uri);
        free(media);
    }
}

struct media_info *media_info(struct media *__restrict media)
{
    return &media->info;
}

const char *media_uri(const struct media *__restrict media)
{
    return media->uri;
}

const char *media_path(const struct media *__restrict media)
{
    return media->path;
}

const char *media_name(const struct media *__restrict media)
{
    return media->name;
}

void media_set_parsed(struct media *__restrict media)
{
    media->parsed = true;
}

bool media_is_parsed(const struct media *__restrict media)
{
    return media->parsed;
}

bool media_contains_path(const struct media *__restrict media, 
                         const char *__restrict path)
{
    if (path_is_absolute(path))
        return strcmp(media->path, path);
    
    char *rpath = realpath(path, NULL);
    if (!rpath)
        return false;
        
    bool ret = strcmp(media->path, rpath) == 0;
        
    free(rpath);
        
    return ret;
}
