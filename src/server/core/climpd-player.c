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

#include <gst/gst.h>

#include <libvci/vector.h>
#include <libvci/random.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include "util/climpd-log.h"
#include "util/terminal-color-map.h"

#include "core/playlist.h"
#include "core/climpd-config.h"
#include "core/media-scheduler.h"
#include "core/media-manager.h"
#include "core/climpd-player.h"

static const char *tag = "climpd-player";
static GstElement *gst_pipeline;
static GstElement *gst_source;
static GstElement *gst_convert;
static GstElement *gst_volume;
static GstElement *gst_sink;
static struct media_scheduler *sched;
static GstState state;
static unsigned int volume;
static bool muted;

extern struct climpd_config conf;

static int climpd_player_play_uri(const char *uri)
{
    GstStateChangeReturn ret;
    
    climpd_player_stop();
    
    g_object_set(gst_source, "uri", uri, NULL);
    
    state = GST_STATE_PLAYING;
    
    ret = gst_element_set_state(gst_pipeline, state);
    if(ret == GST_STATE_CHANGE_FAILURE) {
        climpd_log_e(tag, "unable to play '%s'\n", uri);
        return -EINVAL;
    }
    
    return 0;
}

static void on_pad_added(GstElement *src, GstPad *new_pad, void *data) {
    GstPad *sink_pad;
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps;
    GstStructure *new_pad_struct;
    const gchar *new_pad_type;
    
    sink_pad = gst_element_get_static_pad(gst_convert, "sink");
    
    /* If our converter is already linked, we have nothing to do here */
    if(gst_pad_is_linked(sink_pad))
        goto cleanup1;
    
    /* Check the new pad's type */
    new_pad_caps   = gst_pad_get_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type   = gst_structure_get_name(new_pad_struct);
    
    if(!g_str_has_prefix(new_pad_type, "audio/x-raw")) {
        climpd_log_w(tag, "new pad is no raw audio pad ( %s ) ", new_pad_type);
        climpd_log_append("- ignoring\n");
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

static void on_end_of_stream(void)
{
    const struct media *m;
    int err;
    
    m = media_scheduler_running(sched);
    if(m)
        climpd_log_i(tag, "finished playing '%s'\n", media_info(m)->title);
    
    m = media_scheduler_next(sched);
    if(!m) {
        climpd_log_i(tag, "finished playlist\n");
        return;
    }
    
    err = climpd_player_play_uri(media_uri(m));
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
    
    climpd_log_e("Error received from element %s: %s\n", name, err->message);
    
    if(debug_info) {
        climpd_log_i("Debugging information: %s\n", debug_info);
        g_free(debug_info);
    }

    g_clear_error(&err);
}

static gboolean bus_watcher(GstBus *bus, GstMessage *msg, gpointer data)
{
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            handle_bus_error(msg);
            break;
        case GST_MESSAGE_EOS:
            on_end_of_stream();
            break;
        case GST_MESSAGE_STATE_CHANGED:
        case GST_MESSAGE_TAG:
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

static void print_media(int fd, 
                        unsigned int index, 
                        struct media *__restrict m,
                        const char *__restrict color)
{
    const struct media_info *i;
    unsigned int min, sec;
    int err;
    
    if(!media_is_parsed(m)) {
        err = media_manager_parse_media(m);
        if(err < 0) {
            dprintf(fd, "climpd: failed to parse %s\n", media_path(m));
            return;
        }
    }
    
    i = media_info(m);
    
    min = i->duration / 60;
    sec = i->duration % 60;
    
    dprintf(fd, "%s ( %3u )    %2u:%02u   %-*.*s %-*.*s %-*.*s\n" 
            COLOR_CODE_DEFAULT, 
            color, index, min, sec,
            conf.media_meta_length, conf.media_meta_length, i->title, 
            conf.media_meta_length, conf.media_meta_length, i->artist, 
            conf.media_meta_length, conf.media_meta_length, i->album);
}

int climpd_player_init(struct playlist *pl, bool repeat, bool shuffle)
{
    static const char *elements[] = {
        "uridecodebin",         "gst_source",
        "audioconvert",         "gst_convert",
        "volume",               "gst_volume",
        "autoaudiosink",        "gst_sink",
    };
    GstElement *ele;
    GstBus *bus;
    bool ok;
    int i, err;
    
    gst_pipeline = gst_pipeline_new(NULL);
    if(!gst_pipeline) {
        climpd_log_e(tag, "creating gstreamer pipeline failed\n");
        return -ENOMEM;
    }
    
    for(i = 0; i < ARRAY_SIZE(elements); i += 2) {
        ele = gst_element_factory_make(elements[i], elements[i + 1]);
        if(!ele) {
            climpd_log_e(tag, "creating \"%s\" element failed\n", elements[i]);
            goto cleanup1;
        }
        
        ok = gst_bin_add(GST_BIN(gst_pipeline), ele);
        if(!ok) {
            climpd_log_e(tag, "adding \"%s\" element failed\n", elements[i]);
            gst_object_unref(ele);
            goto cleanup1;
        }
    }
    
    gst_source  = gst_bin_get_by_name(GST_BIN(gst_pipeline), "gst_source");
    gst_convert = gst_bin_get_by_name(GST_BIN(gst_pipeline), "gst_convert");
    gst_volume  = gst_bin_get_by_name(GST_BIN(gst_pipeline), "gst_volume");
    gst_sink    = gst_bin_get_by_name(GST_BIN(gst_pipeline), "gst_sink");
    
    g_signal_connect(gst_source, "pad-added", G_CALLBACK(&on_pad_added), NULL);
    
    ok = gst_element_link(gst_convert, gst_volume);
    if(!ok) {
        climpd_log_e(tag, "linking gst_convert and gst_volume failed\n");
        goto cleanup2;
    }
    
    ok = gst_element_link(gst_volume, gst_sink);
    if(!ok) {
        climpd_log_e(tag, "lining gst_volume and gst_sink failed\n");
        goto cleanup2;
    }
    
    /* init bus */
    bus = gst_element_get_bus(gst_pipeline);
    gst_bus_add_watch(bus, &bus_watcher, NULL);
    gst_object_unref(bus);
    
    state  = GST_STATE_NULL;
    volume = 100;
    muted  = false;
    
    climpd_player_set_volume(volume);

    sched = media_scheduler_new(pl);
    if(!sched) {
        err = -errno;
        goto cleanup2;
    }
    
    media_scheduler_set_repeat(sched, repeat);
    media_scheduler_set_shuffle(sched, shuffle);

    climpd_log_i(tag, "initialized\n");
    
    return 0;

cleanup2:
    gst_object_unref(gst_sink);
    gst_object_unref(gst_volume);
    gst_object_unref(gst_convert);
    gst_object_unref(gst_source);
cleanup1:
    gst_object_unref(gst_pipeline);
    
    return err;
}

void climpd_player_destroy(void)
{
    climpd_player_stop();
    
    media_scheduler_delete(sched);
    gst_object_unref(gst_sink);
    gst_object_unref(gst_volume);
    gst_object_unref(gst_convert);
    gst_object_unref(gst_source);
    gst_object_unref(gst_pipeline);
    
    climpd_log_i(tag, "destroyed\n");
}

int climpd_player_play_media(struct media *m)
{
    int err;
    
    err = media_scheduler_set_running_media(sched, m);
    if(err < 0)
        return err;
    
    return climpd_player_play_uri(media_uri(m));
}

int climpd_player_play_track(unsigned int index)
{
    struct playlist *pl;
    struct media *m;
    int err;
    
    index -= 1;
    
    pl = media_scheduler_playlist(sched);
    
    if(index >= playlist_size(pl))
        return -EINVAL;
    
    err = media_scheduler_set_running_track(sched, index);
    if(err < 0)
        return err;
    
    m = playlist_at(pl, index);
    
    return climpd_player_play_uri(media_uri(m));
}

int climpd_player_play(void)
{
    struct media *m;

    if(climpd_player_playing())
        return 0;

    m = media_scheduler_next(sched);
    if(!m)
        return -ENOENT;
    
    return climpd_player_play_uri(media_uri(m));
}

void climpd_player_pause(void)
{
    if(state == GST_STATE_PAUSED)
        return;
    
    state = GST_STATE_PAUSED;
    
    gst_element_set_state(gst_pipeline, state);
}

void climpd_player_stop(void)
{
    if(state == GST_STATE_NULL)
        return;
    
    state = GST_STATE_NULL;
    
    gst_element_set_state(gst_pipeline, state);
}

int climpd_player_next(void)
{
    struct playlist *pl;
    const struct media *m;
    
    pl = media_scheduler_playlist(sched);
    
    if(playlist_empty(pl))
        return -ENOENT;
    
    m = media_scheduler_next(sched);
    if(!m) {
        climpd_player_stop();
        return 0;
    }
    
    return climpd_player_play_uri(media_uri(m));
}

int climpd_player_previous(void)
{
    struct playlist *pl;
    const struct media *m;
    
    pl = media_scheduler_playlist(sched);

    if(playlist_empty(pl))
        return -ENOENT;
    
    m = media_scheduler_previous(sched);
    if(!m) {
        climpd_player_stop();
        return 0;
    }
    
    return climpd_player_play_uri(media_uri(m));
}

int climpd_player_insert_media(struct media *m)
{
    return media_scheduler_insert_media(sched, m);
}

void climpd_player_take_media(struct media *m)
{
    struct playlist *pl;
    struct media *next;
    
    pl = media_scheduler_playlist(sched);
    
    if(m == media_scheduler_running(sched)) {
        if(playlist_size(pl) == 1) {
            climpd_player_stop();
            media_scheduler_take_media(sched, m);
            return;
        }

        /* 
         * In shuffle mode it is possible at the end of the playlist 
         * to get the same media twice in a row, we must avoid this 
         * since we cannot remove the running media from the
         * scheduler.
         */
        do {
            next = media_scheduler_next(sched);
        } while(next == m);

        if(next)
            climpd_player_play_uri(media_uri(next));
        else
            climpd_player_stop();
    }
    
    media_scheduler_take_media(sched, m);
}

void climpd_player_set_volume(unsigned int vol)
{;
    vol = max(vol, 0);
    vol = min(vol, 100);
    
    volume = vol;
    
    g_object_set(gst_volume, "volume", (double) volume / 100.0f, NULL);
}

unsigned int climpd_player_volume(void)
{
    return volume;
}

void climpd_player_set_muted(bool m)
{
    muted = m;
    
    g_object_set(gst_volume, "mute", muted, NULL);
}

bool climpd_player_muted(void)
{
    return muted;
}

void climpd_player_set_repeat(bool repeat)
{
    media_scheduler_set_repeat(sched, repeat);
}

bool climpd_player_repeat(void)
{
    return media_scheduler_repeat(sched);
}

void climpd_player_set_shuffle(bool shuffle)
{
    media_scheduler_set_shuffle(sched, shuffle);
}

bool climpd_player_shuffle(void)
{
    return media_scheduler_shuffle(sched);
}

bool climpd_player_playing(void)
{
    return state == GST_STATE_PLAYING;
}

bool climpd_player_paused(void)
{
    return state == GST_STATE_PAUSED;
}

bool climpd_player_stopped(void)
{
    return state == GST_STATE_NULL;
}

const struct media *climpd_player_running(void)
{
    return media_scheduler_running(sched);
}

int climpd_player_set_playlist(struct playlist *pl)
{
    int err;
    
    if(media_scheduler_playlist(sched) == pl)
        return 0;
    
    err = media_scheduler_set_playlist(sched, pl);
    if(err < 0)
        return err;
    
    if(climpd_player_playing()) {
        climpd_player_stop();
        
        return climpd_player_play();
    }
    
    return 0;
}

void climpd_player_print_playlist(int fd)
{
    struct playlist *pl;
    struct media **m, *current;
    unsigned int index;
    
    pl = media_scheduler_playlist(sched);
    current = media_scheduler_running(sched);
    index   = 0;
    
    playlist_for_each(pl, m) {
        index += 1;
        
        if(*m == current)
            print_media(fd, index, *m, conf.media_active_color);
        else
            print_media(fd, index, *m, conf.media_passive_color);
    }
}

void climpd_player_print_files(int fd)
{
    struct playlist *pl;
    const struct media **m;
    
    pl = media_scheduler_playlist(sched);
    
    playlist_for_each(pl, m)
        dprintf(fd, "%s\n", (*m)->path);
}

void climpd_player_print_current_track(int fd)
{
    struct playlist *pl;
    struct media *m;
    unsigned int index;
    
    pl = media_scheduler_playlist(sched);
    m =  media_scheduler_running(sched);
    if(!m) {
        dprintf(fd, "No current track available\n");
        return;
    }
    
    index = playlist_index_of(pl, m);
    if(index < 0)
        climpd_log_w("Current track %s not in playlist\n", m->path);
    
    /* silly humans begin to count with '1' */
    index += 1;
    
    print_media(fd, index, m, conf.media_active_color);
}

void climpd_player_print_volume(int fd)
{
    dprintf(fd, " Volume: %u\n", volume);
}

void climpd_player_print_state(int fd)
{
    const struct media *m;
    
    m = media_scheduler_running(sched);
    
    if(climpd_player_playing())
        dprintf(fd, " climpd-player playing\t~ %s ~\n", m->path);
    else if(climpd_player_paused())
        dprintf(fd, " climpd-player is paused on\t~ %s ~\n", m->path);
    else if(climpd_player_stopped())
        dprintf(fd, " climpd-player is stopped\n");
    else
        dprintf(fd, " climpd-player state is unknown\n");
}