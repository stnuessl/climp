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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/vector.h>
#include <libvci/random.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include "util/climpd-log.h"
#include "util/terminal-color-map.h"

#include "core/playlist.h"
#include "core/climpd-config.h"
#include "core/climpd-player.h"

static const char *tag = "climpd-player";

static int climpd_player_set_state(struct climpd_player *__restrict cp,
                                   GstState state)
{
    GstStateChangeReturn state_change;
    
    state_change = gst_element_set_state(cp->pipeline, state);
    if(state_change == GST_STATE_CHANGE_FAILURE)
        return -EINVAL;
    
    cp->state = state;

    return 0;
}

static int climpd_player_play_uri(struct climpd_player *__restrict cp,
                                  const char *__restrict uri)
{
    int err;
    
    climpd_player_stop(cp);
    
    g_object_set(cp->source, "uri", uri, NULL);
    
    err = climpd_player_set_state(cp, GST_STATE_PLAYING);
    if(err < 0) {
        climpd_log_e(tag, "unable to play '%s'\n", uri);
        return err;
    }
    
    climpd_log_i(tag, "now playling '%s'\n", uri);
    
    return 0;
}

static void on_pad_added(GstElement *src, GstPad *new_pad, void *data) {
    struct climpd_player *cp = data;
    GstPad *sink_pad;
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps;
    GstStructure *new_pad_struct;
    const gchar *new_pad_type;
    
    (void) src;
        
    sink_pad = gst_element_get_static_pad(cp->convert, "sink");
    
    /* If our converter is already linked, we have nothing to do here */
    if(gst_pad_is_linked(sink_pad))
        goto cleanup1;
    
    /* Check the new pad's type */
    new_pad_caps   = gst_pad_query_caps(new_pad, NULL);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type   = gst_structure_get_name(new_pad_struct);
    
    if(!g_str_has_prefix(new_pad_type, "audio/x-raw")) {
        climpd_log_w(tag, "new pad is no raw audio pad ( %s ) - ignoring\n", 
                     new_pad_type);
        goto cleanup2;
    }
    
    ret = gst_pad_link(new_pad, sink_pad);
    
    if(GST_PAD_LINK_FAILED(ret))
        climpd_log_e(tag, "linking new pad %s failed\n", new_pad_type);
    
cleanup2:
    gst_caps_unref(new_pad_caps);
cleanup1:
    gst_object_unref(sink_pad);
}

static void parse_tags(const GstTagList *list, const gchar *tag, void *data)
{
    struct media_info *m_info;
    const GValue *val;
    int i, num;
    const char *s;
    
    m_info = data;
    
    num = gst_tag_list_get_tag_size(list, tag);
    
    for (i = 0; i < num; ++i) {
        val = gst_tag_list_get_value_index(list, tag, i);
        
        if(strcmp(GST_TAG_TITLE, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m_info->title, s, MEDIA_META_ELEMENT_SIZE);
            
        } else if(strcmp(GST_TAG_ALBUM, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m_info->album, s, MEDIA_META_ELEMENT_SIZE);
            
        } else if(strcmp(GST_TAG_ARTIST, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m_info->artist, s, MEDIA_META_ELEMENT_SIZE);
            
        } else if(strcmp(GST_TAG_TRACK_NUMBER, tag) == 0) {
            m_info->track = g_value_get_uint(val);
            
        }
    }
}

static void handle_end_of_stream(struct climpd_player *__restrict cp)
{
    int err;
    
    if (cp->running_track) {
        climpd_log_i(tag, "finished '%s'\n", media_uri(cp->running_track));
        media_unref(cp->running_track);
    }
    
    cp->running_track = playlist_next(&cp->playlist);
    if(!cp->running_track) {
        climpd_log_i(tag, "finished playing current playlist\n");
        cp->state = GST_STATE_NULL;
        return;
    }
    
    err = climpd_player_play_uri(cp, media_uri(cp->running_track));
    if(err < 0)
        climpd_log_e(tag, "encountered an error - stopping playback\n");
}

static void handle_bus_error(GstMessage *msg)
{
    GError *err;
    gchar *debug_info;
    const char *name;
    
    gst_message_parse_error(msg, &err, &debug_info);
    
    name = GST_OBJECT_NAME(msg->src);
    
    climpd_log_e(tag, "Error received from element %s: %s\n", name, err->message);
    
    if(debug_info) {
        climpd_log_i(tag, "Debugging information: %s\n", debug_info);
        g_free(debug_info);
    }
    
    g_clear_error(&err);
}

static void handle_tag(struct climpd_player *__restrict cp, GstMessage *msg)
{
    struct media *m = cp->running_track;
    GstTagList *tags;
    
    gst_message_parse_tag(msg, &tags);
    
    if(!tags)
        return;

    if(!m)
        goto out;
    
    if(media_is_parsed(m))
        goto out;
    
    gst_tag_list_foreach(tags, &parse_tags, media_info(m));
    
out:
    gst_tag_list_free(tags);
}

static gboolean bus_watcher(GstBus *bus, GstMessage *msg, gpointer data)
{
    struct climpd_player *player = data;
    
    (void) bus;
    
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            handle_bus_error(msg);
            break;
        case GST_MESSAGE_EOS:
            handle_end_of_stream(player);
            break;
        case GST_MESSAGE_TAG:
            handle_tag(player, msg);
            break;
        case GST_MESSAGE_STATE_CHANGED:
        case GST_MESSAGE_WARNING:
        case GST_MESSAGE_INFO:
        case GST_MESSAGE_BUFFERING:
        case GST_MESSAGE_STEP_DONE:
        case GST_MESSAGE_CLOCK_PROVIDE:
        case GST_MESSAGE_CLOCK_LOST:
        case GST_MESSAGE_NEW_CLOCK:
        case GST_MESSAGE_STRUCTURE_CHANGE:
        case GST_MESSAGE_STREAM_STATUS:
        case GST_MESSAGE_APPLICATION:
        case GST_MESSAGE_ELEMENT:
        case GST_MESSAGE_SEGMENT_START:
        case GST_MESSAGE_SEGMENT_DONE:
        case GST_MESSAGE_LATENCY:
        case GST_MESSAGE_ASYNC_START:
        case GST_MESSAGE_ASYNC_DONE:
        case GST_MESSAGE_REQUEST_STATE:
        case GST_MESSAGE_STEP_START:
        case GST_MESSAGE_QOS:
        case GST_MESSAGE_PROGRESS:
        default:
            break;
    }
    
    return TRUE;
}

static void handle_discoverer_info(GstDiscoverer *disc, 
                                   GstDiscovererInfo *info, 
                                   GError *error, 
                                   void *data)
{
    GstDiscovererResult result;
    struct media *m;
    struct media_info *m_info;
    const char *uri;
    const GstTagList *tags;
    
    (void) disc;
    (void) error;
    
    uri    = gst_discoverer_info_get_uri(info);
    result = gst_discoverer_info_get_result(info);
    
    switch(result) {
        case GST_DISCOVERER_URI_INVALID:
            climpd_log_w(tag, "invalid uri '%s'\n", uri);
            break;
        case GST_DISCOVERER_ERROR:
            break;
        case GST_DISCOVERER_TIMEOUT:
            climpd_log_w(tag, "timeout for uri '%s'\n", uri);
            break;
        case GST_DISCOVERER_BUSY:
            climpd_log_w(tag, "busy\n");
            break;
        case GST_DISCOVERER_MISSING_PLUGINS:
            climpd_log_w(tag, "plugins are missing for '%s'\n", uri);
            break;
        case GST_DISCOVERER_OK:
            m      = data;
            m_info = media_info(m);
            
            m_info->seekable = gst_discoverer_info_get_seekable(info);
            m_info->duration = gst_discoverer_info_get_duration(info) / 1e9;
            
            tags = gst_discoverer_info_get_tags(info);
            if(!tags) {
                climpd_log_w(tag, "no tags for discovered uri '%s'\n", uri);
                break;
            }
            
            gst_tag_list_foreach(tags, &parse_tags, m_info);
            
            media_set_parsed(m);
            break;
        default:
            break;
    }
}

int parse_media(struct climpd_player *__restrict cp, struct media *m)
{
    GstDiscovererInfo *info;
    GError *error;
    const char *uri;
    int err;
    
    uri = media_uri(m);
    
    info = gst_discoverer_discover_uri(cp->discoverer, uri, &error);
    if(!info) {
        err = -error->code;
        climpd_log_w(tag, "parsing '%s' failed - %s\n", uri, error->message);
        g_error_free(error);
        return err;
    }
    
    handle_discoverer_info(cp->discoverer, info, error, m);
    
    gst_discoverer_info_unref(info);
    
    return 0;
}

static void print_media(struct climpd_player *__restrict cp,
                        struct media *__restrict m,
                        unsigned int index, 
                        int fd) 
{
    const char *color;
    const struct media_info *i;
    unsigned int min, sec, meta_len;
    int err;
    
    if(!media_is_parsed(m)) {
        err = parse_media(cp, m);
        if(err < 0) {
            dprintf(fd, "climpd: failed to parse %s\n", media_path(m));
            return;
        }
    }
    
    i = media_info(m);
    
    min = i->duration / 60;
    sec = i->duration % 60;
    
    meta_len = cp->config.media_meta_length;
    
    if (isatty(fd)) {
        if (m == cp->running_track)
            color = cp->config.media_active_color;
        else
            color = cp->config.media_passive_color;
        
        dprintf(fd, "%s ( %3u )    %2u:%02u   %-*.*s %-*.*s %-*.*s\n" 
                COLOR_CODE_DEFAULT, color, index, min, sec,
                meta_len, meta_len, i->title, 
                meta_len, meta_len, i->artist, 
                meta_len, meta_len, i->album);
    } else {
        dprintf(fd, " ( %3u )    %2u:%02u   %-*.*s %-*.*s %-*.*s\n", 
                index, min, sec,
                meta_len, meta_len, i->title, 
                meta_len, meta_len, i->artist, 
                meta_len, meta_len, i->album);
    }
}

int climpd_player_init(struct climpd_player *__restrict cp,
                       const struct climpd_config *config)
{
    static const char *elements[] = {
        "uridecodebin",         "gst_source",
        "audioconvert",         "gst_convert",
        "volume",               "gst_volume",
        "autoaudiosink",        "gst_sink",
    };
    GstElement *ele;
    GstBus *bus;
    GError *error;
    bool ok;
    int err;

    assert(config && "Invalid argument 'config' (null)");
    
    err = -ENOMEM;
    
    /* init gstreamer pipeline */
    cp->pipeline = gst_pipeline_new(NULL);
    if(!cp->pipeline) {
        climpd_log_e(tag, "creating gstreamer pipeline failed\n");
        return err;
    }
    
    for(unsigned int i = 0; i < ARRAY_SIZE(elements); i += 2) {
        ele = gst_element_factory_make(elements[i], elements[i + 1]);
        if(!ele) {
            climpd_log_e(tag, "creating \"%s\" element failed\n", elements[i]);
            goto cleanup1;
        }
        
        ok = gst_bin_add(GST_BIN(cp->pipeline), ele);
        if(!ok) {
            gst_object_unref(ele);
            climpd_log_e(tag, "adding \"%s\" element failed\n", elements[i]);
            goto cleanup1;
        }
    }
    
    cp->source  = gst_bin_get_by_name(GST_BIN(cp->pipeline), "gst_source");
    cp->convert = gst_bin_get_by_name(GST_BIN(cp->pipeline), "gst_convert");
    cp->volume  = gst_bin_get_by_name(GST_BIN(cp->pipeline), "gst_volume");
    cp->sink    = gst_bin_get_by_name(GST_BIN(cp->pipeline), "gst_sink");
    
    g_signal_connect(cp->source, "pad-added", G_CALLBACK(&on_pad_added), cp);
    
    ok = gst_element_link(cp->convert, cp->volume);
    if(!ok) {
        climpd_log_e(tag, "linking gst_convert and gst_volume failed\n");
        goto cleanup2;
    }
    
    ok = gst_element_link(cp->volume, cp->sink);
    if(!ok) {
        climpd_log_e(tag, "linking gst_volume and gst_sink failed\n");
        goto cleanup2;
    }
    
    bus = gst_element_get_bus(cp->pipeline);
    gst_bus_add_watch(bus, &bus_watcher, cp);
    gst_object_unref(bus);
    
    /* init discoverer */
    cp->discoverer = gst_discoverer_new(5 * GST_SECOND, &error);
    if(!cp->discoverer) {
        err = -error->code;
        climpd_log_e(tag, "creating discoverer failed - %s\n", error->message);
        g_error_free(error);
        goto cleanup2;
    }
    
    err = playlist_init(&cp->playlist, config->shuffle, config->repeat);
    if (err < 0) {
        climpd_log_e(tag, "creating playlist failed - %s\n", strerr(-err));
        goto cleanup3;
    }
    
    cp->running_track = NULL;
    
    cp->state = GST_STATE_NULL;
    
    climpd_player_set_volume(cp, config->volume);
    
    cp->config.media_active_color  = config->media_active_color;
    cp->config.media_passive_color = config->media_passive_color;
    cp->config.media_meta_length   = config->media_meta_length;
    

    climpd_log_i(tag, "initialized\n");
    
    return 0;

cleanup3:
    gst_object_unref(cp->discoverer);
cleanup2:
    gst_object_unref(cp->sink);
    gst_object_unref(cp->volume);
    gst_object_unref(cp->convert);
    gst_object_unref(cp->source);
cleanup1:
    gst_object_unref(cp->pipeline);
    
    return err;
}

void climpd_player_destroy(struct climpd_player *__restrict cp)
{
    
    if (cp->running_track)
        media_unref(cp->running_track);
    
    climpd_player_stop(cp);
    
    playlist_destroy(&cp->playlist);
    
    gst_object_unref(cp->discoverer);
    gst_object_unref(cp->sink);
    gst_object_unref(cp->volume);
    gst_object_unref(cp->convert);
    gst_object_unref(cp->source);
    gst_object_unref(cp->pipeline);
        
    climpd_log_i(tag, "destroyed\n");
}

int climpd_player_play_path(struct climpd_player *__restrict cp,
                            const char *path)
{
    struct media *m;
    unsigned int index;
    int err;
    
    index = playlist_index_of_path(&cp->playlist, path);
    if (index != (unsigned int) -1)
        return climpd_player_play_track(cp, (int) index);

    m = media_new(path);
    if (!m)
        return -errno;
    
    err = playlist_insert_back(&cp->playlist, m);
    if (err < 0)
        goto cleanup1;
    
    if (cp->running_track)
        media_unref(cp->running_track);
    
    cp->running_track = m;
    
    err = climpd_player_play_uri(cp, media_uri(m));
    if (err < 0)
        goto cleanup1;
    
    return 0;
    
cleanup1:
    media_unref(m);
    return err;
}

int climpd_player_play_track(struct climpd_player *__restrict cp, int index)
{
    if ((unsigned int) abs(index) >= playlist_size(&cp->playlist))
        return -EINVAL;
    
    if (cp->running_track)
        media_unref(cp->running_track);
    
    cp->running_track = playlist_at(&cp->playlist, index);
    
    return climpd_player_play_uri(cp, media_uri(cp->running_track));
}

int climpd_player_play(struct climpd_player *__restrict cp)
{
    int err;
    
    if(climpd_player_is_playing(cp))
        return 0;
    
    if (climpd_player_is_paused(cp)) {
        const char *uri = media_uri(cp->running_track);
        
        err = climpd_player_set_state(cp, GST_STATE_PLAYING);
        if(err < 0) {
            climpd_log_e(tag, "failed to resume '%s'.\n", uri);
            return err;
        }
        
        climpd_log_i(tag, "resuming '%s'.\n", uri); 
        return 0;
    }
    
    assert(climpd_player_is_stopped(cp) && "INVALID PLAYER STATE");
    
    if (cp->running_track)
        media_unref(cp->running_track);
    
    cp->running_track = playlist_next(&cp->playlist);
    if (!cp->running_track)
        return -ENOENT;
    
    return climpd_player_play_uri(cp, media_uri(cp->running_track));
}

void climpd_player_pause(struct climpd_player *__restrict cp)
{
    int err;
    
    if(cp->state == GST_STATE_PAUSED)
        return;
    
    err = climpd_player_set_state(cp, GST_STATE_PAUSED);
    if(err < 0)
        climpd_log_e(tag, "unable to enter paused state.\n");
}

void climpd_player_stop(struct climpd_player *__restrict cp)
{
    int err;
    
    if(cp->state == GST_STATE_NULL)
        return;
    
    err = climpd_player_set_state(cp, GST_STATE_NULL);
    if(err < 0)
        climpd_log_e(tag, "unable to enter stopped state.\n");
}

int climpd_player_next(struct climpd_player *__restrict cp)
{
    if (cp->running_track)
        media_unref(cp->running_track);
    
    cp->running_track = playlist_next(&cp->playlist);

    if (!cp->running_track) {
        climpd_player_stop(cp);
        
        if (playlist_empty(&cp->playlist))
            return -ENOENT;
        else
            return 0;
    }
    
    return climpd_player_play_uri(cp, media_uri(cp->running_track));
}

int climpd_player_previous(struct climpd_player *__restrict cp)
{
    (void) cp;
    assert(0 && "NOT IMPLEMENTED");
    
    return 0;
}

int climpd_player_peek(struct climpd_player *__restrict cp)
{
    bool ok;
    gint64 val;

    ok = gst_element_query_position(cp->pipeline, GST_FORMAT_TIME, &val);
    if (!ok) {
        climpd_log_e(tag, "failed to query the position of the stream\n");
        return -ENOTSUP;
    }
    
    return (int) (val / (int) 1e9);
}

int climpd_player_seek(struct climpd_player *__restrict cp, unsigned int val)
{
    const struct media_info *i;
    GstFormat format;
    GstSeekFlags flags;
    long time;
    bool ok;
        
    i = media_info(cp->running_track);
    
    if (!i->seekable)
        return -ENOTSUP;
    
    if (val >= i->duration)
        return -EINVAL;
    
    format = GST_FORMAT_TIME;
    flags  = GST_SEEK_FLAG_FLUSH;
    
    /* convert to nanoseconds */
    time = (long) val * (unsigned int) 1e9;
    
    ok = gst_element_seek_simple(cp->pipeline, format, flags, time);
    
    if(!ok) {
        climpd_log_e(tag, "seeking to position %d failed\n", val);
        return -ENOMEM;
    }
    
    return 0;
}

int climpd_player_insert(struct climpd_player *__restrict cp, 
                         const char *__restrict path)
{
    return playlist_emplace_back(&cp->playlist, path);
}

void climpd_player_delete_index(struct climpd_player *__restrict cp, int index)
{
    if ((unsigned int) abs(index) < playlist_size(&cp->playlist))
        media_unref(playlist_take(&cp->playlist, (unsigned int) index));
}

void climpd_player_set_volume(struct climpd_player *__restrict cp, 
                              unsigned int vol)
{
    double volume;
    
    vol = max(vol, 0);
    vol = min(vol, 100);
    
    cp->volume_val = vol;

    volume = (100.0 - 50 * log10(101.0 - vol)) / 100;
        
    g_object_set(cp->volume, "volume", volume, NULL);
}

unsigned int climpd_player_volume(const struct climpd_player *__restrict cp)
{
    return cp->volume_val;
}

void climpd_player_set_muted(struct climpd_player *__restrict cp, bool mute)
{        
    g_object_set(cp->volume, "mute", mute, NULL);
}

bool climpd_player_muted(const struct climpd_player *__restrict cp)
{
    bool mute;
    
    g_object_get(cp->volume, "mute", &mute, NULL);
    
    return mute;
}

bool climpd_player_toggle_muted(struct climpd_player *__restrict cp)
{
    bool mute = !climpd_player_muted(cp);
    
    climpd_player_set_muted(cp, mute);
    
    return mute;
}

void climpd_player_set_repeat(struct climpd_player *__restrict cp, bool repeat)
{
    playlist_set_repeat(&cp->playlist, repeat);
}

bool climpd_player_repeat(const struct climpd_player *__restrict cp)
{
    return playlist_repeat(&cp->playlist);
}

bool climpd_player_toggle_repeat(struct climpd_player *__restrict cp)
{
    return playlist_toggle_repeat(&cp->playlist);
}

void climpd_player_set_shuffle(struct climpd_player *__restrict cp, 
                               bool shuffle)
{
    playlist_set_shuffle(&cp->playlist, shuffle);
}

bool climpd_player_shuffle(const struct climpd_player *__restrict cp)
{
    return playlist_shuffle(&cp->playlist);
}

bool climpd_player_toggle_shuffle(struct climpd_player *__restrict cp)
{
    return playlist_toggle_shuffle(&cp->playlist);
}

bool climpd_player_is_playing(const struct climpd_player *__restrict cp)
{
    return cp->state == GST_STATE_PLAYING;
}

bool climpd_player_is_paused(const struct climpd_player *__restrict cp)
{
    return cp->state == GST_STATE_PAUSED;
}

bool climpd_player_is_stopped(const struct climpd_player *__restrict cp)
{
    return cp->state == GST_STATE_NULL;
}

int climpd_player_add_media_list(struct climpd_player *__restrict cp,
                                 struct media_list *__restrict ml)
{
    int err = playlist_add_media_list(&cp->playlist, ml);
    if (err < 0)
        climpd_log_e(tag, "adding media list failed - %s\n", strerr(-err));
        
    return err;
}

int climpd_player_set_media_list(struct climpd_player *__restrict cp,
                                 struct media_list *__restrict ml)
{
    int err;
    
    playlist_clear(&cp->playlist);
    
    err = playlist_add_media_list(&cp->playlist, ml);
    if (err < 0) {
        climpd_log_e(tag, "setting new media list failed - %s :: playlist lost",
                    strerr(-err));
    }
    
    return err;
}

void climpd_player_clear_playlist(struct climpd_player *__restrict cp)
{
    playlist_clear(&cp->playlist);
}

void climpd_player_print_playlist(struct climpd_player *__restrict cp, int fd)
{
    unsigned int size = playlist_size(&cp->playlist);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = playlist_at(&cp->playlist, i);
        
        print_media(cp, m, i, fd);
        
        media_unref(m);
    }
    
    dprintf(fd, "\n");
}

void climpd_player_print_files(struct climpd_player *__restrict cp, int fd)
{
    unsigned int size = playlist_size(&cp->playlist);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = playlist_at(&cp->playlist, i);
        
        dprintf(fd, "%s\n", media_path(m));
        
        media_unref(m);
    }
    
    dprintf(fd, "\n");
}

void climpd_player_print_running_track(struct climpd_player *__restrict cp, 
                                       int fd)
{
    unsigned int index;

    
    index = playlist_index_of(&cp->playlist, cp->running_track);
    if(index == (unsigned int) -1) {
        const char *path = media_path(cp->running_track);
        climpd_log_i("Current track %s not in playlist\n", path);
    }
    
    print_media(cp, cp->running_track, index, fd);
}

void climpd_player_print_volume(struct climpd_player *__restrict cp, int fd)
{
    unsigned int volume;
    
    volume = climpd_player_volume(cp);
    
    dprintf(fd, " Volume: %u\n", volume);
}
