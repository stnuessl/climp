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

#include <math.h>
#include <errno.h>

#include <libvci/macro.h>

#include <core/media-player/media-player.h>
#include <core/climpd-log.h>
#include <core/util.h>

static const char *tag = "media-player";

static GstElement *_gst_pipeline;
static GstElement *_gst_source;
static GstElement *_gst_convert;
static GstElement *_gst_volume;
static GstElement *_gst_sink;
static GstState _gst_state;
static unsigned int _volume;
static bool _muted;
static void (*_on_eos)(void);



static int media_player_set_state(GstState state)
{
    GstStateChangeReturn val;
    
    val = gst_element_set_state(_gst_pipeline, state);
    
    return (val == GST_STATE_CHANGE_FAILURE) ? -1 : 0;
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

static void handle_bus_error(GstMessage *msg)
{
    GError *err;
    gchar *debug_info;
    const char *name;
    
    gst_message_parse_error(msg, &err, &debug_info);
    
    name = GST_OBJECT_NAME(msg->src);
    
    climpd_log_e(tag, "received error from \"%s\": %s\n", name, err->message);
    
    if (debug_info) {
        climpd_log_i(tag, "additional debugging information: %s\n", debug_info);
        g_free(debug_info);
    }
    
    g_clear_error(&err);
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
        _on_eos();
        break;
    case GST_MESSAGE_TAG:
    case GST_MESSAGE_STATE_CHANGED:
        gst_message_parse_state_changed(msg, NULL, &_gst_state, NULL);
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

void media_player_init(void (*on_eos)(void))
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
    if (!_gst_pipeline) {
        climpd_log_e(tag, "creating gstreamer pipeline failed\n");
        goto fail;
    }
    
    for (unsigned int i = 0; i < ARRAY_SIZE(elements); i += 2) {
        ele = gst_element_factory_make(elements[i], elements[i + 1]);
        if (!ele) {
            climpd_log_e(tag, "creating \"%s\" element failed\n", elements[i]);
            goto fail;
        }
        
        ok = gst_bin_add(GST_BIN(_gst_pipeline), ele);
        if (!ok) {
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
    if (!ok) {
        climpd_log_e(tag, "linking gst_convert and gst_volume failed\n");
        goto fail;
    }
    
    ok = gst_element_link(_gst_volume, _gst_sink);
    if (!ok) {
        climpd_log_e(tag, "linking gst_volume and gst_sink failed\n");
        goto fail;
    }
    
    bus = gst_element_get_bus(_gst_pipeline);
    gst_bus_add_watch(bus, &bus_watcher, NULL);
    gst_object_unref(bus);

    _gst_state = GST_STATE_NULL;
    _on_eos = on_eos;
    
    media_player_set_volume(0);
    
    climpd_log_i(tag, "initialized\n");
    
    return;
    
fail:
    die_failed_init(tag);
}
}

void media_player_destroy(void)
{
    media_player_stop();
    
    gst_object_unref(_gst_sink);
    gst_object_unref(_gst_volume);
    gst_object_unref(_gst_convert);
    gst_object_unref(_gst_source);
    gst_object_unref(_gst_pipeline);
    
    climpd_log_i(tag, "destroyed\n");
}

int media_player_play(const char *__restrict uri)
{
    int err;
    
    g_object_set(_gst_source, "uri", uri, NULL);
    
    err = media_player_set_state(GST_STATE_PLAYING);
    if (err < 0) {
        climpd_log_e(tag, "unable to play '%s'\n", uri);
        return err;
    }
    
    climpd_log_i(tag, "now playing '%s'\n", uri);
    
    return 0;
}

void media_player_pause(void)
{
    int err;
    
    if(_gst_state == GST_STATE_PAUSED)
        return;
    
    err = media_player_set_state(GST_STATE_PAUSED);
    if(err < 0)
        climpd_log_e(tag, "unable to pause playback.\n");
}

void media_player_stop(void)
{
    int err;
    
    if(_gst_state == GST_STATE_NULL)
        return;
    
    err = media_player_set_state(GST_STATE_NULL);
    if(err < 0)
        climpd_log_e(tag, "unable to stop playback.\n");
}

int media_player_peek(void)
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

int media_player_seek(unsigned int sec)
{
    GstFormat format;
    GstSeekFlags flags;
    long time;
    bool ok;

    format = GST_FORMAT_TIME;
    flags  = GST_SEEK_FLAG_FLUSH;
    
    /* convert to nanoseconds */
    time = (long) sec * (unsigned int) 1e9;
    
    ok = gst_element_seek_simple(_gst_pipeline, format, flags, time);
    if(!ok) {
        int m = sec / 60;
        int s = sec % 60;
        
        climpd_log_e(tag, "seeking to position '%d:%d' failed\n", m, s);
        return -EAGAIN;
    }
    
    return 0;
}

void media_player_set_volume(unsigned int vol)
{
    double val;
    
    vol = max(vol, 0);
    vol = min(vol, 100);
    
    _volume = vol;
    
    val = (100.0 - 50 * log10(101.0 - vol)) / 100;
    
    g_object_set(_gst_volume, "volume", val, NULL);
}

unsigned int media_player_volume(void)
{
    return _volume;
}

void media_player_set_muted(bool muted)
{
    _muted = muted;
    
    g_object_set(_gst_volume, "mute", muted, NULL);
}

bool media_player_muted(void)
{
    return _muted;
}

enum media_player_state media_player_state(void)
{
    return _gst_state;
}