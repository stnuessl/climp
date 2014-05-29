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

#include <string.h>
#include <errno.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/map.h>
#include <libvci/hash.h>
#include <libvci/compare.h>

#include "../media-objects/media.h"

#include "climpd-log.h"

static GstDiscoverer *discoverer;
static struct map *media_map;
static const char *tag = "media-creator";

static void parse_tags(const GstTagList *list, const gchar *tag, void *data)
{
    struct media *m;
    const GValue *val;
    int i, num;
    const char *s;
    
    m  = data;
    
    num = gst_tag_list_get_tag_size(list, tag);
    
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
            
        } else if(strcmp(GST_TAG_TRACK_NUMBER, tag) == 0) {
            m->info.track = g_value_get_uint(val);
            
        }
    }
}

static void on_discovered(GstDiscoverer *disc, 
                          GstDiscovererInfo *info, 
                          GError *error, 
                          void *data)
{
    GstDiscovererResult result;
    struct media *m;
    const char *uri;
    const GstTagList *tags;
    
    uri    = gst_discoverer_info_get_uri(info);
    result = gst_discoverer_info_get_result(info);
    
    switch(result) {
    case GST_DISCOVERER_URI_INVALID:
        climpd_log_w(tag, "invalid uri '%s'\n", uri);
        break;
    case GST_DISCOVERER_ERROR:
        climpd_log_w(tag, "error for uri '%s' - %s\n", uri, error->message);
        break;
    case GST_DISCOVERER_TIMEOUT:
        climpd_log_w(tag, "timeout for uri '%s'\n", uri);
        break;
    case GST_DISCOVERER_BUSY:
        climpd_log_w(tag, "busy\n");
        break;
    case GST_DISCOVERER_MISSING_PLUGINS:
//         climpd_log_w(tag, "missing plugins for '%s':\n", uri);
//         
//         txt = gst_discoverer_info_get_missing_elements_installer_details(info);
//         if(!txt) {
//             climpd_log_w(tag, "no details on missing plugins available\n");
//             break;
//         }
//         
//         for(i = 0; txt[i]; ++i)
//             climpd_log_w(tag, "%s\n", txt[i]);
//         
//         g_strfreev(txt);
        break;
    case GST_DISCOVERER_OK:
        m = map_take(media_map, uri);
        if(!m) {
            climpd_log_w(tag, "no media for discovered uri '%s'\n", uri);
            return;
        }
        
        m->info.seekable = gst_discoverer_info_get_seekable(info);
        m->info.duration = gst_discoverer_info_get_duration(info) / 1e9;
        
        tags = gst_discoverer_info_get_tags(info);
        if(!tags) {
            climpd_log_w(tag, "no tags for discovered uri '%s'\n", uri);
            break;
        }
        
        gst_tag_list_foreach(tags, &parse_tags, m);
        break;
    default:
        break;
    }
}

static void on_finished(GstDiscoverer *discoverer, void *data)
{
    /* clear all media which's uri did not cause a GST_DISCOVERER_OK */ 
    map_clear(media_map);
}

int media_creator_init(void)
{
    media_map = map_new(0, &compare_string, &hash_string);
    if(!media_map)
        return -errno;
    
    discoverer = gst_discoverer_new(10 * GST_SECOND, NULL);
    if(!discoverer) {
        map_delete(media_map);
        errno = ENOMEM;
        return -errno;
    }
    
    g_signal_connect(discoverer, "discovered", G_CALLBACK(on_discovered), NULL);
    g_signal_connect(discoverer, "finished", G_CALLBACK(on_finished), NULL);

    gst_discoverer_start(discoverer);
    
    return 0;
}

void media_creator_destroy(void)
{
    gst_discoverer_stop(discoverer);
    g_object_unref(discoverer);
    map_delete(media_map);
}

struct media *media_creator_new_media(const char *path)
{
    struct media *m;
    int err;
    
    m = media_new(path);
    if(!m)
        return NULL;
    
    err = map_insert(media_map, m->uri, m);
    if(err < 0)
        goto cleanup1;
    
    err = gst_discoverer_discover_uri_async(discoverer, m->uri);
    if(err == 0) {
        climpd_log_e(tag, "failed to discover URI '%s'\n", m->uri);
        errno = ENOMEM;
        goto cleanup2;
    }
    
    return m;

cleanup2:
    map_take(media_map, m->uri);
cleanup1:
    media_delete(m);
    
    return NULL;
}

void media_creator_delete_media(struct media *m)
{
    map_take(media_map, m->uri);
    media_delete(m);
}