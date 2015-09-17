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

#include <errno.h>
#include <stdbool.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/map.h>
#include <libvci/compare.h>
#include <libvci/hash.h>
#include <libvci/error.h>

#include <core/media-player/media-discoverer.h>
#include <core/climpd-log.h>
#include <core/util.h>

static const char *tag = "media-discoverer";

static struct map _media_map;
static GstDiscoverer _gst_disc;

static const char *gst_discoverer_result_str(GstDiscovererResult result)
{
    static const char *table[] = {
        [GST_DISCOVERER_URI_INVALID]     = "invalid uri",
        [GST_DISCOVERER_ERROR]           = "internal discoverer error",
        [GST_DISCOVERER_TIMEOUT]         = "timeout",
        [GST_DISCOVERER_MISSING_PLUGINS] = "missing plugins",
    };
    
    return table[result];
}

void handle_missing_plugins(GstDiscovererInfo *info)
{
    const char **msg;
    
    msg = gst_discoverer_info_get_missing_elements_installer_details(info);
    
    for (int i = 0; msg[i]; ++i) 
        climpd_log_append("%s", msg[i]);
    
    climpd_log_append("\n");
}

static void parse_tags(const GstTagList *list, const gchar *tag, void *data)
{
    struct media_info *m_info = data;
    const GValue *val;
    const char *s;
    int i, num;
    
    num = gst_tag_list_get_tag_size(list, tag);
    
    for (i = 0; i < num; ++i) {
        val = gst_tag_list_get_value_index(list, tag, i);
        
        if(strcmp(GST_TAG_TITLE, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m_info->title, s, MEDIA_META_ELEMENT_SIZE);
            
            m_info->title[MEDIA_META_ELEMENT_SIZE - 1] = '\0';
        } else if(strcmp(GST_TAG_ALBUM, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m_info->album, s, MEDIA_META_ELEMENT_SIZE);
            
            m_info->album[MEDIA_META_ELEMENT_SIZE - 1] = '\0';
        } else if(strcmp(GST_TAG_ARTIST, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m_info->artist, s, MEDIA_META_ELEMENT_SIZE);
            
            m_info->artist[MEDIA_META_ELEMENT_SIZE - 1] = '\0';
        } else if(strcmp(GST_TAG_TRACK_NUMBER, tag) == 0) {
            m_info->track = g_value_get_uint(val);
            
        }
    }
}

static void parse_info(GstDiscovererInfo *info, struct media *__restrict m)
{
    const GstTagList *tags;
    struct media_info *m_info;
    
    m_info = media_info(m);
    m_info->seekable = gst_discoverer_info_get_seekable(info);
    m_info->duration = gst_discoverer_info_get_duration(info) / 1e9;
    
    tags = gst_discoverer_info_get_tags(info);
    
    if (tags)
        gst_tag_list_foreach(tags, &parse_tags, m_info);
    
    media_set_parsed(m, true);
}

static void on_discovered(GstDiscoverer *disc, 
                           GstDiscovererInfo *info,
                           GError *error,
                           void *data)
{
    GstDiscovererResult result;
    struct media *m;
    const char *uri;
    
    (void) disc;
    (void) data;
    
    result = gst_discoverer_info_get_result(info);
    uri = gst_discoverer_info_get_uri(info);
    m = map_take(&_media_map, uri);
    
    if (!m || m->parsed)
        return;
    
    if (result != GST_DISCOVERER_OK) {
        climpd_log_e(tag, "unable to discover '%s'", uri);
        
        if (error)
            climpd_log_append(" - %s\n", error->message);
        else
            climpd_log_append(" - %s\n", gst_discoverer_result_str(result));
        
        if (result == GST_DISCOVERER_MISSING_PLUGINS)
            handle_missing_plugins(info);
        
        goto out;
    }
    
    parse_info(info, m);
    
out:
    media_unref(m);
}

void media_discoverer_init(void)
{
    const struct map_config conf = {
        .map         = MAP_DEFAULT_SIZE,
        .lower_bound = MAP_DEFAULT_LOWER_BOUND,
        .upper_bound = MAP_DEFAULT_UPPER_BOUND,
        .static_size = false,
        .key_compare = &compare_string,
        .key_hash    = &hash_string,
        .data_delete = (void (*)(void *)) &media_unref;
    };
    GError *gerror;
    int err;
    
    err = map_init(&_media_map, &conf);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize map - %s\n", errstr(-err));
        goto fail;
    }
    
    _gst_disc = gst_discoverer_new(5 * GST_SECOND, &gerror);
    if (!_gst_disc) {
        climpd_log_e(tag, "failed to initialize GstDiscoverer");
        
        if (gerror) {
            climpd_log_append(" - %s", gerror->message);
            
            g_error_free(gerror);
        }
        
        climpd_log_append("\n");
        
        goto fail;
    }
    
    g_signal_connect(_gst_disc, "discovered", G_CALLBACK(on_discovered), NULL);
    
    gst_discoverer_start(_gst_disc);
    
    climpd_log_i(tag, "initialized\n");
    
    return;
    
fail:
    die_failed_init(tag);
}

void media_discoverer_destroy(void)
{
    gst_discoverer_stop(_gst_disc);
    
    map_destroy(&_media_map);
    g_object_unref(_gst_disc);
    
    climpd_log_i(tag, "destroyed\n");
}

int media_discoverer_discover_media_sync(struct media* m)
{
    GstDiscovererResult result;
    GstDiscovererInfo *info;
    GError *error;
    const char *uri;
    
    if (m->parsed)
        return 0;
    
    uri = media_uri(m);
    
    info = gst_discoverer_discover_uri(_gst_disc, uri, &error);
    if (!info) {
        climpd_log_e(tag, "failed to discover '%s'", uri);
        
        if (error) {
            climpd_log_append(" - %s\n", error->message);
            
            g_error_free(error);
        }
        
        climpd_log_append("\n");
        return -1;
    }
    
    parse_info(info, m);
    
    gst_discoverer_info_unref(info);
    
    return 0;
}

void media_discoverer_discover_media_async(struct media *m)
{
    const char *uri;
    bool ok;
    int err;
    
    media_ref(m);
    
    uri = media_uri(m);
    
    err = map_insert(&_media_map, uri, m);
    if (err < 0) {
        climpd_log_w(tag, "failed to discover '%s' - %s\n", uri, errstr(-err));
        media_unref(m);
        
        return;
    }
    
    ok = gst_discoverer_discover_uri_async(_gst_disc, uri);
    if (!ok) {
        climpd_log_w(tag, "failed to async discover '%s'\n", uri);
        map_take(&_media_map, uri);
        media_unref(m);
        
        return;
    }
}

void media_discoverer_discover_media_list_async(struct media_list *ml)
{
    unsigned int i, size;
    
    size = media_list_size(ml);
    
    for (i = 0; i < size; ++i) {
        struct media *m;
        const char *uri;
        int err;
        bool ok;
        
        m = media_list_at(ml, i);
        uri = media_uri(m);
        
        err = map_insert(&_media_map, uri, m);
        if (err < 0) {
            climpd_log_w(tag, "failed to discover '%s' - %s\n", uri, 
                         strerr(-err));
            media_unref(m);
            goto fail;
        }
        
        ok = gst_discoverer_discover_uri_async(_gst_disc, uri);
        if (!ok) {
            climpd_log_w(tag, "failed to async discover '%s'\n", uri);
            map_take(&_media_map, uri);
            media_unref(m);
            goto fail;
        }
    }
    
    return;
    
fail:
    while (i--) {
        struct media *m1 = media_list_at(ml, i);
        struct media *m2 = map_take(&_media_map, media_uri(m1));
        
        media_unref(m2);
        media_unref(m1);
    }
}
