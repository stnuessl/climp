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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/limits.h>

#include <libvci/filesystem.h>

#include <obj/media.h>
#include <obj/uri.h>

#define MEDIA_META_ELEMENT_SIZE  64

struct media *media_new(const char *__restrict arg)
{
    struct media *media;
    
    media = malloc(sizeof(*media));
    if(!media)
        return NULL;
    
    media->uri = uri_new(arg);
    if(!media->uri)
        goto cleanup1;
    
    strncpy(media->info.title,  media->uri, MEDIA_META_ELEMENT_SIZE);
    strncpy(media->info.artist, "",         MEDIA_META_ELEMENT_SIZE);
    strncpy(media->info.album,  "",         MEDIA_META_ELEMENT_SIZE);
    
    media->info.track = 0;
    media->info.duration = 0;
    media->info.seekable = false;
        
    media->hier = uri_hierarchical(media->uri);
    
    /* 
     * Webradio, etc. won't (can't ?) be parsed so we prevent this by
     * setting this flag to true if applicable
     */
    media->parsed = uri_is_http(media->uri);
    
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
        uri_delete(media->uri);
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

const char *media_hierarchical(const struct media *__restrict media)
{
    return media->hier;
}

void media_set_parsed(struct media *__restrict media, bool val)
{
    media->parsed = val;
}

bool media_is_parsed(const struct media *__restrict media)
{
    return media->parsed;
}

int media_compare(const struct media *__restrict m1, 
                  const struct media *__restrict m2)
{
    return strverscmp(m1->hier, m2->hier);
}

int media_hierarchical_compare(const struct media *__restrict media, 
                               const char *__restrict arg)
{
    static __thread char rpath[PATH_MAX];
    
    if (uri_ok(arg))
        arg = uri_hierarchical(arg);
    else if (!path_is_absolute(arg))
        arg = realpath(arg, rpath);
        
    return (arg) ? strcmp(media->hier, arg) : 1;
}

