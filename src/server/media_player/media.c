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

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>


#include "media.h"

#define MEDIA_FILE_PREFIX        "file://"
#define MEDIA_META_ELEMENT_SIZE  64


static void media_parse_tags(const GstTagList *list, 
                             const gchar *tag, 
                             gpointer data)
{
    struct media *m;
    const GValue *val;
    int i, num;
    const char *s;

    m  = data;
    
    num = gst_tag_list_get_tag_size (list, tag);
    
    for (i = 0; i < num; ++i) {
        val = gst_tag_list_get_value_index(list, tag, i);

        if(strcmp(GST_TAG_TITLE, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m->info.title, s, MEDIA_META_ELEMENT_SIZE);
        } else if(strcmp(GST_TAG_ALBUM, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m->info.album, s, MEDIA_META_ELEMENT_SIZE);
        } else if(strcmp(GST_TAG_ARTIST, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m->info.artist, s, MEDIA_META_ELEMENT_SIZE);
        }
    }
}

static struct media *media_new_abs_path(const char *path)
{
    GstDiscoverer *discoverer;
    GstDiscovererInfo *info;
    GstDiscovererResult result;
    const GstTagList *tags;
    struct media *media;
    
    media = malloc(sizeof(*media));
    if(!media)
        goto out;
    
    media->uri = malloc(sizeof(MEDIA_FILE_PREFIX) + strlen(path));
    if(!media->uri)
        goto cleanup1;
    
    media->uri = strcpy(media->uri, MEDIA_FILE_PREFIX);
    media->uri = strcat(media->uri, path);
    
    media->path = media->uri + sizeof(MEDIA_FILE_PREFIX) - 1;

    discoverer = gst_discoverer_new(GST_SECOND, NULL);
    if(!discoverer) {
        errno = ENOMEM;
        goto cleanup2;
    }
    
    info = gst_discoverer_discover_uri(discoverer, media->uri, NULL);
    
    result = gst_discoverer_info_get_result(info);
    if(result != GST_DISCOVERER_OK) {
        errno = EINVAL;
    }
    tags = gst_discoverer_info_get_tags(info);
    if(!tags) {
        strncpy(media->info.title, "Unknown", MEDIA_META_ELEMENT_SIZE);
        strncpy(media->info.artist, "Unknown", MEDIA_META_ELEMENT_SIZE);
        strncpy(media->info.album, "Unknown", MEDIA_META_ELEMENT_SIZE);
        
        return media;
    }
    
    gst_tag_list_foreach(tags, media_parse_tags, media);
    
    gst_discoverer_info_unref(info);
    g_object_unref(discoverer);
    
    return media;
    
cleanup2:
    free(media->uri);
cleanup1:
    free(media);
out:
    return NULL;
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