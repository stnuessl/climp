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

#include <string.h>
#include <errno.h>

#include <libvci/compare.h>
#include <libvci/hash.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/playlist/tag-reader.h>

static const char *tag = "tag-reader";

static const char *stringify_discoverer_result(GstDiscovererResult result)
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
    struct tag_reader *reader = data;
    GstDiscovererResult result;
    struct media *m;
    const char *uri;
    
    (void) disc;
    
    result = gst_discoverer_info_get_result(info);
    uri = gst_discoverer_info_get_uri(info);
    
    m = map_take(&reader->media_map, uri);
    if (!m)
        return;
    
    if (m->parsed)
        goto out;
    
    if (result != GST_DISCOVERER_OK) {
        climpd_log_e(tag, "unable to discover '%s'", uri);
        
        if (error)
            climpd_log_append(" - %s\n", error->message);
        else
            climpd_log_append(" - %s\n", stringify_discoverer_result(result));
        
        if (result == GST_DISCOVERER_MISSING_PLUGINS)
            handle_missing_plugins(info);
        
        goto out;
    }
    
    parse_info(info, m);
    
out:
    media_unref(m);
}

static void on_start(GstDiscoverer *disc, void *data)
{
    struct tag_reader *tr = data;
    
    (void) disc;
    (void) tr;
}

static void on_finished(GstDiscoverer *disc, void *data)
{
    struct tag_reader *tr = data;
    
    (void) disc;
    (void) tr;
}

int tag_reader_init(struct tag_reader *__restrict tr)
{
    const struct map_config conf = {
        .size        = MAP_DEFAULT_SIZE,
        .lower_bound = MAP_DEFAULT_LOWER_BOUND,
        .upper_bound = MAP_DEFAULT_UPPER_BOUND,
        .static_size = false,
        .key_compare = &compare_string,
        .key_hash    = &hash_string,
        .data_delete = (void (*)(void *)) &media_unref,
    };
    GError *error;
    int err;
    
    err = map_init(&tr->media_map, &conf);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize map - %s\n", strerr(-err));
        return -err;
    }
    
    tr->disc = gst_discoverer_new(5 * GST_SECOND, &error);
    if (!tr->disc) {
        if (error) {
            err = -error->code;
            climpd_log_e(tag, "failed to initialize async discoverer - %s\n",
                         error->message);
            g_error_free(error);
        } else {
            err = -ENOTSUP;
            climpd_log_e(tag, "failed to initialize async discoverer\n");
        }

        map_destroy(&tr->media_map);
        return err;
    }
    
    g_signal_connect(tr->disc, "discovered", G_CALLBACK(on_discovered), tr);
    g_signal_connect(tr->disc, "starting", G_CALLBACK(on_start), tr);
    g_signal_connect(tr->disc, "finished", G_CALLBACK(on_finished), tr);
    
    gst_discoverer_start(tr->disc);

    climpd_log_i(tag, "initialized\n");
    
    return 0;
}

void tag_reader_destroy(struct tag_reader *__restrict tr)
{
    gst_discoverer_stop(tr->disc);
    
    g_object_unref(tr->disc);
    map_destroy(&tr->media_map);
    
    climpd_log_i(tag, "destroyed\n");
}

void tag_reader_read_async(struct tag_reader *__restrict tr, struct media *m)
{
    const char *uri = media_uri(m);
    bool ok;
    int err;
    
    if (media_is_parsed(m))
        return;
    
    media_ref(m);
    
    err = map_insert(&tr->media_map, uri, m);
    if (err < 0) {
        climpd_log_w(tag, "failed to async read tags for '%s' - %s\n", uri, 
                     strerr(-err));
        media_unref(m);
        
        return;
    }
    
    ok = gst_discoverer_discover_uri_async(tr->disc, uri);
    if (!ok) {
        climpd_log_w(tag, "failed to async read tags for '%s'\n", uri);
        map_take(&tr->media_map, uri);
        media_unref(m);
        
        return;
    }
}