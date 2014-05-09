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

#define MEDIA_PLAYER_MAX_VOLUME         120
#define MEDIA_PLAYER_MIN_VOLUME         0

static void media_player_on_end_of_stream(struct media_player *__restrict mp)
{
    media_player_next(mp);
}

static gboolean bus_watcher(GstBus *bus, GstMessage *msg, gpointer data)
{
    struct media_player *mp;
    
    mp = data;
    
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
        if(mp->on_bus_error)
            mp->on_bus_error(mp, msg);
        break;
    case GST_MESSAGE_EOS:
        if(mp->on_end_of_stream)
            mp->on_end_of_stream(mp);
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

    mp->playbin2 = gst_element_factory_make("playbin2", NULL);
    if(!mp->playbin2)
        return -ENOMEM;
    
    bus = gst_element_get_bus(mp->playbin2);
    
    gst_bus_add_watch(bus, &bus_watcher, mp);

    mp->state           = GST_STATE_NULL;
    mp->volume          = 100;
    mp->muted           = false;
    
    g_object_set(mp->playbin2, "volume", (double) mp->volume / 100.0f, NULL);
    
    return 0;
}

void media_player_destroy(struct media_player *__restrict mp)
{
    media_player_stop(mp);
    
    gst_object_unref(mp->playbin2);
}

void media_player_play_media(struct media_player *__restrict mp,
                             const struct media *m)
{
    int err;
    
    assert(m && "No media passed");

    media_player_stop(mp);
    
    g_object_set(mp->playbin2, "uri", m->uri, NULL);
    
    mp->state = GST_STATE_PLAYING;
    
    gst_element_set_state(mp->playbin2, GST_STATE_PLAYING);
}

void media_player_pause(struct media_player *__restrict mp)
{  
    if(mp->state == GST_STATE_PAUSED)
        return;
    
    mp->state = GST_STATE_PAUSED;
    
    gst_element_set_state(mp->playbin2, GST_STATE_PAUSED);
}

void media_player_stop(struct media_player *__restrict mp)
{
    if(mp->state == GST_STATE_NULL)
        return;
    
    mp->state = GST_STATE_NULL;
    
    gst_element_set_state(mp->playbin2, GST_STATE_NULL);
}

void media_player_set_muted(struct media_player *__restrict mp, bool muted)
{
    mp->muted = muted
}

bool media_player_muted(const struct media_player *__restrict mp)
{
    return mp->muted;
}

void media_player_set_volume(struct media_player *__restrict mp,
                             unsigned int volume)
{
    mp->volume = volume;
    g_object_set(mp->playbin2, "volume", (double) volume / 100.0f, NULL);
}

unsigned int media_player_volume(const struct media_player *__restrict mp)
{
    return mp->volume;
}

bool media_player_playing(const struct media_player *__restrict mp)
{
    return mp->state == GST_STATE_PLAYING;
}

bool media_player_paused(const struct media_player *__restrict mp)
{
    return mp->state == GST_STATE_PAUSED;
}

bool media_player_stopped(const struct media_player *__restrict mp)
{
    return mp->state == GST_STATE_NULL;
}

void media_player_on_end_of_stream(struct media_player *__restrict mp,
                                   void (*func)(struct media_player *__restrict mp))
{
    mp->on_end_of_stream = func;
}

void media_player_on_bus_error(struct media_player *__restrict mp,
                               void (*func)(struct media_player *, GstMessage *))
{
    mp->on_bus_error = func;
}