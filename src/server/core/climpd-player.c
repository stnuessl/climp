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
//  * You should have received a copy of the GNU General Public License
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

#include <core/climpd-log.h>
#include <core/terminal-color-map.h>
#include <core/climpd-player.h>

#include <obj/playlist.h>

static const char *tag = "climpd-player";

static GstElement *_gst_pipeline;
static GstElement *_gst_source;
static GstElement *_gst_convert;
static GstElement *_gst_volume;
static GstElement *_gst_sink;
static GstDiscoverer *_gst_discoverer;
static GstState _gst_state;
static unsigned int _volume;
static struct playlist _playlist;
static struct media *_running_track;

static int climpd_player_set_state(GstState state)
{
    GstStateChangeReturn state_change;
    
    state_change = gst_element_set_state(_gst_pipeline, state);
    if(state_change == GST_STATE_CHANGE_FAILURE)
        return -EINVAL;
    
    _gst_state = state;

    return 0;
}

static int climpd_player_play_uri(const char *__restrict uri)
{
    int err;
    
    climpd_player_stop();
    
    g_object_set(_gst_source, "uri", uri, NULL);
    
    err = climpd_player_set_state(GST_STATE_PLAYING);
    if(err < 0) {
        climpd_log_e(tag, "unable to play '%s'\n", uri);
        return err;
    }
    
    return 0;
}

static void on_pad_added(GstElement *src, GstPad *new_pad, void *data) {
    GstPad *sink_pad;
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps;
    GstStructure *new_pad_struct;
    const gchar *new_pad_type;
    
    (void) data;
    (void) src;
        
    sink_pad = gst_element_get_static_pad(_gst_convert, "sink");
    
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

static void handle_end_of_stream(void)
{
    int err;
    
    if (_running_track) {
        climpd_log_i(tag, "finished '%s'\n", media_uri(_running_track));
        media_unref(_running_track);
    }
    
    _running_track = playlist_next(&_playlist);
    if(!_running_track) {
        climpd_log_i(tag, "finished playing current playlist\n");
        _gst_state = GST_STATE_NULL;
        return;
    }
    
    err = climpd_player_play_uri(media_uri(_running_track));
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
    
    climpd_log_e(tag, "Error received from element %s: %s\n", name, 
                 err->message);
    
    if(debug_info) {
        climpd_log_i(tag, "Debugging information: %s\n", debug_info);
        g_free(debug_info);
    }
    
    g_clear_error(&err);
}

static void handle_tag(GstMessage *msg)
{
    struct media *m = _running_track;
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
    
    (void) bus;
    (void) data;
    
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            handle_bus_error(msg);
            break;
        case GST_MESSAGE_EOS:
            handle_end_of_stream();
            break;
        case GST_MESSAGE_TAG:
            handle_tag(msg);
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

int parse_media(struct media *m)
{
    GstDiscovererInfo *info;
    GError *error;
    const char *uri;
    int err;
    
    uri = media_uri(m);
    
    info = gst_discoverer_discover_uri(_gst_discoverer, uri, &error);
    if(!info) {
        err = -error->code;
        climpd_log_w(tag, "parsing '%s' failed - %s\n", uri, error->message);
        g_error_free(error);
        return err;
    }
    
    handle_discoverer_info(_gst_discoverer, info, error, m);
    
    gst_discoverer_info_unref(info);
    
    return 0;
}

static void print_media(struct media *__restrict m, unsigned int index, int fd) 
{
    const char *color;
    const struct media_info *i;
    unsigned int min, sec, meta_len;
    int err;
    
    if(!media_is_parsed(m)) {
        err = parse_media(m);
        if(err < 0) {
            dprintf(fd, "climpd: failed to parse %s\n", media_path(m));
            return;
        }
    }
    
    i = media_info(m);
    
    min = i->duration / 60;
    sec = i->duration % 60;
    
    meta_len = climpd_config_media_meta_length();
    
    if (isatty(fd)) {
        if (m == _running_track)
            color = climpd_config_media_active_color();
        else
            color = climpd_config_media_passive_color();
        
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

void climpd_player_init(void)
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

    _gst_pipeline = gst_pipeline_new(NULL);
    if(!_gst_pipeline) {
        climpd_log_e(tag, "creating gstreamer pipeline failed\n");
        goto fail;
    }
    
    for(unsigned int i = 0; i < ARRAY_SIZE(elements); i += 2) {
        ele = gst_element_factory_make(elements[i], elements[i + 1]);
        if(!ele) {
            climpd_log_e(tag, "creating \"%s\" element failed\n", elements[i]);
            goto fail;
        }
        
        ok = gst_bin_add(GST_BIN(_gst_pipeline), ele);
        if(!ok) {
            gst_object_unref(ele);
            climpd_log_e(tag, "adding \"%s\" element failed\n", elements[i]);
            goto fail;
        }
    }
    
    _gst_source  = gst_bin_get_by_name(GST_BIN(_gst_pipeline), "gst_source");
    _gst_convert = gst_bin_get_by_name(GST_BIN(_gst_pipeline), "gst_convert");
    _gst_volume  = gst_bin_get_by_name(GST_BIN(_gst_pipeline), "gst_volume");
    _gst_sink    = gst_bin_get_by_name(GST_BIN(_gst_pipeline), "gst_sink");
    
    g_signal_connect(_gst_source, "pad-added", G_CALLBACK(&on_pad_added), NULL);
    
    ok = gst_element_link(_gst_convert, _gst_volume);
    if(!ok) {
        climpd_log_e(tag, "linking gst_convert and gst_volume failed\n");
        goto fail;
    }
    
    ok = gst_element_link(_gst_volume, _gst_sink);
    if(!ok) {
        climpd_log_e(tag, "linking gst_volume and gst_sink failed\n");
        goto fail;
    }
    
    bus = gst_element_get_bus(_gst_pipeline);
    gst_bus_add_watch(bus, &bus_watcher, NULL);
    gst_object_unref(bus);
    
    /* init discoverer */
    _gst_discoverer = gst_discoverer_new(5 * GST_SECOND, &error);
    if(!_gst_discoverer) {
        err = -error->code;
        climpd_log_e(tag, "creating discoverer failed - %s\n", error->message);
        g_error_free(error);
        goto fail;
    }
    
    bool shuffle = climpd_config_shuffle();
    bool repeat  = climpd_config_repeat();
    
    err = playlist_init(&_playlist, shuffle, repeat);
    if (err < 0) {
        climpd_log_e(tag, "creating playlist failed - %s\n", strerr(-err));
        goto fail;
    }
    
    _running_track = NULL;
    
    _gst_state = GST_STATE_NULL;
    
    unsigned int vol = climpd_config_volume();
    
    climpd_player_set_volume(vol);
    
    climpd_log_i(tag, "initialized\n");
    
    return;

fail:
    climpd_log_e(tag, "failed to initialize - aborting...\n");
    exit(EXIT_FAILURE);
}

void climpd_player_destroy(void)
{
    
    if (_running_track)
        media_unref(_running_track);
    
    climpd_player_stop();
    
    playlist_destroy(&_playlist);
    
    gst_object_unref(_gst_discoverer);
    gst_object_unref(_gst_sink);
    gst_object_unref(_gst_volume);
    gst_object_unref(_gst_convert);
    gst_object_unref(_gst_source);
    gst_object_unref(_gst_pipeline);
        
    climpd_log_i(tag, "destroyed\n");
}

int climpd_player_play_path(const char *path)
{
    struct media *m;
    unsigned int index;
    int err;
    
    index = playlist_index_of_path(&_playlist, path);
    if (index != (unsigned int) -1)
        return climpd_player_play_track((int) index);

    m = media_new(path);
    if (!m)
        return -errno;
    
    err = playlist_insert_back(&_playlist, m);
    if (err < 0)
        goto cleanup1;
    
    if (_running_track)
        media_unref(_running_track);
    
    _running_track = m;
    
    /* 
     * We don't need to update the index since it is at the last position
     * and after that song the playback would finish.
     */
    
    err = climpd_player_play_uri(media_uri(m));
    if (err < 0)
        goto cleanup1;
    
    return 0;
    
cleanup1:
    media_unref(m);
    return err;
}

int climpd_player_play_track(int index)
{
    if ((unsigned int) abs(index) >= playlist_size(&_playlist))
        return -EINVAL;
    
    if (_running_track)
        media_unref(_running_track);
    
    _running_track = playlist_at(&_playlist, index);
    playlist_update_index(&_playlist, index);
    
    return climpd_player_play_uri(media_uri(_running_track));
}

int climpd_player_play(void)
{
    const char *uri;
    int err;
    
    switch (_gst_state) {
    case GST_STATE_PLAYING:
        break;
    case GST_STATE_PAUSED:
        uri = media_uri(_running_track);
        
        err = climpd_player_set_state(GST_STATE_PLAYING);
        if(err < 0) {
            climpd_log_e(tag, "failed to resume '%s'.\n", uri);
            return err;
        }
        
        climpd_log_i(tag, "resuming '%s'.\n", uri); 
        break;
    case GST_STATE_NULL:
    default:
        if (_running_track)
            media_unref(_running_track);
        
        _running_track = playlist_next(&_playlist);
        if (!_running_track)
            return -ENOENT;
        
        uri = media_uri(_running_track);
        err = climpd_player_play_uri(uri);
        if (err < 0) {
            climpd_log_e(tag, "failed to start playback - %s\n", strerr(-err));
            return err;
        }
        
        climpd_log_i(tag, "started playback of '%s'\n", uri);
        break;
    }
    
    return 0;
}

void climpd_player_pause(void)
{
    int err;
    
    if(_gst_state == GST_STATE_PAUSED)
        return;
    
    err = climpd_player_set_state(GST_STATE_PAUSED);
    if(err < 0)
        climpd_log_e(tag, "unable to enter paused state.\n");
}

void climpd_player_stop(void)
{
    int err;
    
    if(_gst_state == GST_STATE_NULL)
        return;
    
    err = climpd_player_set_state(GST_STATE_NULL);
    if(err < 0)
        climpd_log_e(tag, "unable to enter stopped state.\n");
}

int climpd_player_next(void)
{
    if (_running_track)
        media_unref(_running_track);
    
    _running_track = playlist_next(&_playlist);

    if (!_running_track) {
        climpd_player_stop();
        
        if (playlist_empty(&_playlist))
            return -ENOENT;
        else
            return 0;
    }
    
    return climpd_player_play_uri(media_uri(_running_track));
}

int climpd_player_previous(void)
{
    assert(0 && "NOT IMPLEMENTED");
    
    return 0;
}

int climpd_player_peek(void)
{
    bool ok;
    gint64 val;

    ok = gst_element_query_position(_gst_pipeline, GST_FORMAT_TIME, &val);
    if (!ok) {
        climpd_log_e(tag, "failed to query the position of the stream\n");
        return -ENOTSUP;
    }
    
    return (int) (val / (int) 1e9);
}

int climpd_player_seek(unsigned int val)
{
    const struct media_info *i;
    GstFormat format;
    GstSeekFlags flags;
    long time;
    bool ok;
        
    i = media_info(_running_track);
    
    if (!i->seekable)
        return -ENOTSUP;
    
    if (val >= i->duration)
        return -EINVAL;
    
    format = GST_FORMAT_TIME;
    flags  = GST_SEEK_FLAG_FLUSH;
    
    /* convert to nanoseconds */
    time = (long) val * (unsigned int) 1e9;
    
    ok = gst_element_seek_simple(_gst_pipeline, format, flags, time);
    
    if(!ok) {
        climpd_log_e(tag, "seeking to position %d failed\n", val);
        return -ENOMEM;
    }
    
    return 0;
}

int climpd_player_insert(const char *__restrict path)
{
    return playlist_emplace_back(&_playlist, path);
}

void climpd_player_delete_index(int index)
{
    if ((unsigned int) abs(index) < playlist_size(&_playlist))
        media_unref(playlist_take(&_playlist, index));
}

void climpd_player_delete_media(const struct media *__restrict m)
{
    unsigned int index = playlist_index_of(&_playlist, m);
    if (index == (unsigned int) -1)
        return;
    
    media_unref(playlist_take(&_playlist, (int) index));
}

void climpd_player_set_volume(unsigned int vol)
{
    double val;
    
    vol = max(vol, 0);
    vol = min(vol, 100);
    
    _volume = vol;

    val = (100.0 - 50 * log10(101.0 - vol)) / 100;
        
    g_object_set(_gst_volume, "volume", val, NULL);
}

unsigned int climpd_player_volume(void)
{
    return _volume;
}

void climpd_player_set_muted(bool mute)
{        
    g_object_set(_gst_volume, "mute", mute, NULL);
}

bool climpd_player_muted(void)
{
    bool mute;
    
    g_object_get(_gst_volume, "mute", &mute, NULL);
    
    return mute;
}

bool climpd_player_toggle_muted(void)
{
    bool mute = !climpd_player_muted();
    
    climpd_player_set_muted(mute);
    
    return mute;
}

void climpd_player_set_repeat(bool repeat)
{
    playlist_set_repeat(&_playlist, repeat);
}

bool climpd_player_repeat(void)
{
    return playlist_repeat(&_playlist);
}

bool climpd_player_toggle_repeat(void)
{
    return playlist_toggle_repeat(&_playlist);
}

void climpd_player_set_shuffle(bool shuffle)
{
    playlist_set_shuffle(&_playlist, shuffle);
}

bool climpd_player_shuffle(void)
{
    return playlist_shuffle(&_playlist);
}

bool climpd_player_toggle_shuffle(void)
{
    return playlist_toggle_shuffle(&_playlist);
}

enum climpd_player_state climpd_player_state(void)
{
    return _gst_state;
}

bool climpd_player_is_playing(void)
{
    return _gst_state == GST_STATE_PLAYING;
}

bool climpd_player_is_paused(void)
{
    return _gst_state == GST_STATE_PAUSED;
}

bool climpd_player_is_stopped(void)
{
    return _gst_state == GST_STATE_NULL;
}

int climpd_player_add_media_list(struct media_list *__restrict ml)
{
    int err = playlist_add_media_list(&_playlist, ml);
    if (err < 0)
        climpd_log_e(tag, "adding media list failed - %s\n", strerr(-err));
        
    return err;
}

int climpd_player_set_media_list(struct media_list *__restrict ml)
{
    int err;
    
    playlist_clear(&_playlist);
    
    err = playlist_add_media_list(&_playlist, ml);
    if (err < 0) {
        climpd_log_e(tag, "setting new media list failed - %s :: playlist lost",
                    strerr(-err));
    }
    
    return err;
}

void climpd_player_remove_media_list(struct media_list *__restrict ml)
{
    playlist_remove_media_list(&_playlist, ml);
}

void climpd_player_clear_playlist(void)
{
    playlist_clear(&_playlist);
}

void climpd_player_print_playlist(int fd)
{
    unsigned int size = playlist_size(&_playlist);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = playlist_at(&_playlist, i);
        
        print_media(m, i, fd);
        
        media_unref(m);
    }
    
    dprintf(fd, "\n");
}

void climpd_player_print_files(int fd)
{
    unsigned int size = playlist_size(&_playlist);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = playlist_at(&_playlist, i);
        
        dprintf(fd, "%s\n", media_path(m));
        
        media_unref(m);
    }
    
    dprintf(fd, "\n");
}

void climpd_player_print_running_track(int fd)
{
    unsigned int index;

    
    index = playlist_index_of(&_playlist, _running_track);
    if(index == (unsigned int) -1) {
        const char *path = media_path(_running_track);
        climpd_log_i("Current track %s not in playlist\n", path);
    }
    
    print_media(_running_track, index, fd);
}

void climpd_player_print_volume(int fd)
{
    dprintf(fd, " Volume: %u\n", _volume);
}
