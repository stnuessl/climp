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
#include <math.h>

#include <libvci/macro.h>

#include <core/climpd-log.h>
#include <core/audio-player/gst-engine.h>

static const char *tag = "gst-engine";

static void handle_bus_error(struct gst_engine *__restrict en, GstMessage *msg)
{
    GError *err;
    char *name, *info;
    
    if (en->on_bus_error) {
        name = GST_OBJECT_NAME(msg->src);
        
        gst_message_parse_error(msg, &err, &info);
        
        en->on_bus_error(en, name, err->message, info);
        
        if (info)
            g_free(info);
        
        if (err)
            g_error_free(err);
    }
}

static gboolean bus_watcher(GstBus *bus, GstMessage *msg, void *data)
{
    struct gst_engine *en = data;
    
    (void) bus;
    
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
        handle_bus_error(en, msg);
        break;
    case GST_MESSAGE_EOS:
        if (en->on_end_of_stream)
            en->on_end_of_stream(en);
        break;
    case GST_MESSAGE_STATE_CHANGED:
        gst_message_parse_state_changed(msg, NULL, &en->gst_state, NULL);
        break;
    case GST_MESSAGE_TAG:
    case GST_MESSAGE_PROGRESS:
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
    default:
        break;
    }
    
    return TRUE;
}

static int gst_engine_set_state(struct gst_engine *__restrict en, 
                                GstState state)
{
    GstStateChangeReturn val;
    
    val = gst_element_set_state(en->gst_pipeline, state);
    
    return (val == GST_STATE_CHANGE_FAILURE) ? -1 : 0;
}

static void on_pad_added(GstElement *src, GstPad *new_pad, void *data) {
    struct gst_engine *en = data;
    GstPad *sink_pad;
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps;
    GstStructure *new_pad_struct;
    const gchar *new_pad_type;
    
    (void) src;
    
    sink_pad = gst_element_get_static_pad(en->gst_convert, "sink");
    
    /* If our converter is already linked, we have nothing to do here */
    if(gst_pad_is_linked(sink_pad))
        goto cleanup1;
    
    /* Check the new pad's type */
    new_pad_caps   = gst_pad_query_caps(new_pad, NULL);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type   = gst_structure_get_name(new_pad_struct);
    
    if(!g_str_has_prefix(new_pad_type, "audio/x-raw"))
        goto cleanup2;
    
    ret = gst_pad_link(new_pad, sink_pad);
    
    if(GST_PAD_LINK_FAILED(ret))
        climpd_log_e(tag, "linking new pad %s failed\n", new_pad_type);
    
cleanup2:
    gst_caps_unref(new_pad_caps);
cleanup1:
    gst_object_unref(sink_pad);
}

int gst_engine_init(struct gst_engine *__restrict en)
{
    static const char *elements[] = {
        "uridecodebin",         "source",
        "audioconvert",         "convert",
        "pitch",                "pitch",
        "volume",               "volume",
        "autoaudiosink",        "sink",
    };
    GstElement *ele;
    GstBus *bus;
    bool ok;
    
    memset(en, 0, sizeof(*en));
    
    en->gst_pipeline = gst_pipeline_new(NULL);
    if (!en->gst_pipeline) {
        climpd_log_e(tag, "creating gstreamer pipeline failed\n");
        goto fail;
    }
    
    for (unsigned int i = 0; i < ARRAY_SIZE(elements); i += 2) {
        ele = gst_element_factory_make(elements[i], elements[i + 1]);
        if (!ele) {
            climpd_log_e(tag, "creating \"%s\" element failed\n", elements[i]);
            goto fail;
        }
        
        ok = gst_bin_add(GST_BIN(en->gst_pipeline), ele);
        if (!ok) {
            gst_object_unref(ele);
            climpd_log_e(tag, "adding \"%s\" element failed\n", elements[i]);
            goto fail;
        }
    }
    
    en->gst_source = gst_bin_get_by_name(GST_BIN(en->gst_pipeline), "source");
    en->gst_convert = gst_bin_get_by_name(GST_BIN(en->gst_pipeline), "convert");
    en->gst_pitch = gst_bin_get_by_name(GST_BIN(en->gst_pipeline), "pitch");
    en->gst_volume = gst_bin_get_by_name(GST_BIN(en->gst_pipeline), "volume");
    en->gst_sink = gst_bin_get_by_name(GST_BIN(en->gst_pipeline), "sink");
    
    g_signal_connect(en->gst_source, "pad-added", G_CALLBACK(&on_pad_added), en);
    
    ok = gst_element_link(en->gst_convert, en->gst_pitch);
    if (!ok) {
        climpd_log_e(tag, "linking gst_convert and gst_pitch failed\n");
        goto fail;
    }
    
    ok = gst_element_link(en->gst_pitch, en->gst_volume);
    if (!ok) {
        climpd_log_e(tag, "linking gst_pitch and gst_volume failed\n");
        goto fail;
    }
    
    ok = gst_element_link(en->gst_volume, en->gst_sink);
    if (!ok) {
        climpd_log_e(tag, "linking gst_volume and gst_sink failed\n");
        goto fail;
    }
    
    
    bus = gst_element_get_bus(en->gst_pipeline);
    gst_bus_add_watch(bus, &bus_watcher, en);
    gst_object_unref(bus);
    
    en->gst_state = GST_STATE_NULL;

    gst_engine_set_volume(en, 0);
    
    climpd_log_i(tag, "initialized\n");

    return 0;
    
fail:
    if (en->gst_sink)
        g_object_unref(en->gst_sink);
    
    if (en->gst_volume)
        g_object_unref(en->gst_volume);
    
    if (en->gst_pitch)
        g_object_unref(en->gst_pitch);
    
    if (en->gst_convert)
        g_object_unref(en->gst_convert);
    
    if (en->gst_source)
        g_object_unref(en->gst_source);
    
    if (en->gst_pipeline)
        g_object_unref(en->gst_pipeline);
    
    climpd_log_e(tag, "initialization failed\n");
    
    return -1;
}

void gst_engine_destroy(struct gst_engine *__restrict en)
{
    gst_engine_stop(en);
    
    g_object_unref(en->gst_sink);
    g_object_unref(en->gst_volume);
    g_object_unref(en->gst_pitch);
    g_object_unref(en->gst_convert);
    g_object_unref(en->gst_source);
    g_object_unref(en->gst_pipeline);
    
    climpd_log_i(tag, "destroyed\n");
}

void gst_engine_set_uri(struct gst_engine *__restrict en, 
                        const char *__restrict uri)
{
    g_object_set(en->gst_source, "uri", uri, NULL);
}

int gst_engine_play(struct gst_engine *__restrict en)
{
    int err;
    
    if (en->gst_state == GST_STATE_PLAYING)
        return 0;

    err = gst_engine_set_state(en, GST_STATE_PLAYING);
    if (err < 0)
        climpd_log_e(tag, "failed to start playback\n");
    
    return err;
}

int gst_engine_pause(struct gst_engine *__restrict en)
{
    int err;
    
    if(en->gst_state == GST_STATE_PAUSED)
        return 0;
    
    if (en->gst_state == GST_STATE_NULL)
        return -1;
    
    err = gst_engine_set_state(en, GST_STATE_PAUSED);
    if(err < 0)
        climpd_log_e(tag, "failed to pause playback.\n");
    
    return err;
}

int gst_engine_stop(struct gst_engine *__restrict en)
{
    int err;
    
    if(en->gst_state == GST_STATE_NULL)
        return 0;
    
    err = gst_engine_set_state(en, GST_STATE_NULL);
    if(err < 0)
        climpd_log_e(tag, "failed to stop playback.\n");
    
    return err;
}

int gst_engine_stream_position(const struct gst_engine *__restrict en)
{
    bool ok;
    gint64 val;
    
    ok = gst_element_query_position(en->gst_pipeline, GST_FORMAT_TIME, &val);
    if (!ok) {
        climpd_log_e(tag, "failed to query the position of the stream\n");
        return -1;
    }
    
    return (int) (val / (int) 1e9);
}

int gst_engine_set_stream_position(struct gst_engine *__restrict en, 
                                   unsigned int sec)
{
    GstFormat format;
    GstSeekFlags flags;
    long time;
    bool ok;
    
    format = GST_FORMAT_TIME;
    flags  = GST_SEEK_FLAG_FLUSH;
    
    /* convert to nanoseconds */
    time = (long) sec * (unsigned int) 1e9;
    
    ok = gst_element_seek_simple(en->gst_pipeline, format, flags, time);
    if(!ok) {
        int m = sec / 60;
        int s = sec % 60;
        
        climpd_log_e(tag, "seeking to position '%d:%d' failed\n", m, s);
        return -1;
    }
    
    return 0;
}

void gst_engine_set_pitch(struct gst_engine *__restrict en, float pitch)
{
    pitch = max(pitch, 0.1f);
    pitch = min(pitch, 10.0f);
    
    g_object_set(en->gst_pitch, "pitch", pitch, NULL);
}

float gst_engine_pitch(const struct gst_engine *__restrict en)
{
    float pitch;
    
    g_object_get(en->gst_pitch, "pitch", &pitch, NULL);
    
    return pitch;
}

void gst_engine_set_speed(struct gst_engine *__restrict en, float speed)
{
    speed = max(speed, 0.1f);
    speed = min(speed, 40.0f);
    
    g_object_set(en->gst_pitch, "tempo", speed, NULL);
}

float gst_engine_speed(const struct gst_engine *__restrict en)
{
    float speed;
    
    g_object_get(en->gst_pitch, "tempo", &speed, NULL);
    
    return speed;
}

void gst_engine_set_volume(struct gst_engine *__restrict en, unsigned int vol)
{
    double val;
    
    vol = max(vol, 0);
    vol = min(vol, 100);
    
    en->volume = vol;
    
    val = (101.0 - 50 * log10(101.0 - vol)) / 101;

    g_object_set(en->gst_volume, "volume", val, NULL);
    
    climpd_log_i(tag, "volume changed to '%u'\n", en->volume);
}

unsigned int gst_engine_volume(const struct gst_engine *__restrict en)
{
    return en->volume;
}

void gst_engine_set_mute(struct gst_engine *__restrict en, bool mute)
{
    en->mute = mute;
    
    g_object_set(en->gst_volume, "mute", mute, NULL);
    
    climpd_log_i(tag, "now '%s'\n", (mute) ? "muted" : "unmuted");
}

bool gst_engine_is_muted(const struct gst_engine *__restrict en)
{
    return en->mute;
}

enum gst_engine_state gst_engine_state(const struct gst_engine *__restrict en)
{
    return en->gst_state;
}

void gst_engine_set_end_of_stream_handler(struct gst_engine *__restrict en,
                                          eos_callback func)
{
    en->on_end_of_stream = func;
}

void gst_engine_set_bus_error_handler(struct gst_engine *__restrict en, 
                                      bus_error_callback func)
{
    en->on_bus_error = func;
}