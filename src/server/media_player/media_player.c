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
#include <errno.h>
#include <assert.h>

#include <gst/gst.h>

#include <libvci/macro.h>

#include "media_player.h"

#define MEDIA_PLAYER_GST_ERROR EIO

#define MEDIA_PLAYER_MAX_VOLUME         120
#define MEDIA_PLAYER_MIN_VOLUME         0

static void media_player_on_end_of_stream(struct media_player *__restrict mp)
{
    struct media *m;
    
    media_player_stop(mp);
    
    m = playlist_next(mp->playlist, mp->current_media);
    if(!m)
        return;
    
    media_player_play_media(mp, m);
}

static void media_player_on_state_change(struct media_player *__restrict mp,
                                         GstMessage *msg)
{
    if(GST_MESSAGE_SRC(msg) != GST_OBJECT(mp->playbin2))
        return;
    
    gst_message_parse_state_changed(msg, NULL, &mp->state, NULL);
}

static gboolean bus_watcher(GstBus *bus, GstMessage *msg, gpointer data)
{
    struct media_player *mp;
    GError *err;
    gchar *debug_info;
    
    mp = data;
    
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
        gst_message_parse_error (msg, &err, &debug_info);
        
        g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
        
        g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error (&err);
        g_free (debug_info);
        break;
    case GST_MESSAGE_EOS:
        media_player_on_end_of_stream(mp);
        break;
    case GST_MESSAGE_STATE_CHANGED:
        media_player_on_state_change(mp, msg);
        break;
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

struct media_player *media_player_new(void)
{
    struct media_player *__restrict mp;
    int err;
    
    mp = malloc(sizeof(*mp));
    if(!mp)
        return NULL;
    
    err = media_player_init(mp);
    if(err < 0) {
        free(mp);
        return NULL;
    }
    
    return mp;
}

void media_player_delete(struct media_player *__restrict mp)
{
    media_player_destroy(mp);
    free(mp);
}

int media_player_init(struct media_player *__restrict mp)
{
    GstBus *bus;
    
    mp->playlist = playlist_new();
    if(!mp->playlist)
        return -errno;
    
    mp->playbin2 = gst_element_factory_make("playbin2", NULL);
    if(!mp->playbin2) {
        playlist_delete(mp->playlist);
        return -MEDIA_PLAYER_GST_ERROR;
    }
    
    bus = gst_element_get_bus(mp->playbin2);
    
    gst_bus_add_watch(bus, &bus_watcher, mp);
    
    mp->muted           = false;
    mp->volume          = 100;
    mp->state           = GST_STATE_NULL;
    mp->current_media   = NULL;
    
    return 0;
}

void media_player_destroy(struct media_player *__restrict mp)
{
    media_player_stop(mp);
    
    gst_object_unref(mp->playbin2);
    playlist_delete(mp->playlist);
}

struct media *media_player_current_media(struct media_player *__restrict mp)
{
    return mp->current_media;
}

struct playlist *media_player_playlist(struct media_player *__restrict mp)
{
    return mp->playlist;
}

int media_player_play(struct media_player *__restrict mp)
{
    if(mp->state == GST_STATE_PLAYING)
        return 0;
    
    if(!mp->current_media)
        mp->current_media = playlist_first(mp->playlist);
    
    if(!mp->current_media)
        return -EINVAL;
    
    return media_player_play_media(mp, mp->current_media);
}

int media_player_play_media(struct media_player *__restrict mp,
                            struct media *m)
{
    int err;
    
    assert(m && "No media passed");
        
    if(!playlist_contains_media(mp->playlist, m)) {
        err = playlist_add_media(mp->playlist, m);
        if(err < 0)
            return err;
    }
    
    media_player_stop(mp);
    
    g_object_set(mp->playbin2, "uri", m->uri, NULL);
    
    gst_element_set_state(mp->playbin2, GST_STATE_PLAYING);
    
    mp->current_media   = m;
    
    return 0;
}

void media_player_pause(struct media_player *__restrict mp)
{  
    if(mp->state == GST_STATE_PAUSED)
        return;
    
    gst_element_set_state(mp->playbin2, GST_STATE_PAUSED);
}

void media_player_stop(struct media_player *__restrict mp)
{
    if(mp->state == GST_STATE_NULL)
        return;
    
    gst_element_set_state(mp->playbin2, GST_STATE_NULL);
}

void media_player_toggle_mute(struct media_player *__restrict mp)
{
    mp->muted = !mp->muted;
    
    g_object_set(mp->playbin2, "mute", mp->muted, NULL);
}

void media_player_set_volume(struct media_player *__restrict mp,
                             unsigned int volume)
{
    volume = max(volume, MEDIA_PLAYER_MIN_VOLUME);
    volume = min(volume, MEDIA_PLAYER_MAX_VOLUME);
    
    g_object_set(mp->playbin2, "volume", (double) volume / 100.0f, NULL);
    
    mp->volume = volume;
}

unsigned int media_player_volume(const struct media_player *__restrict mp)
{
    return mp->volume;
}
