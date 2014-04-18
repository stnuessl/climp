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
    media_player_next(mp);
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
    
    mp = data;
    
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
        if(mp->on_bus_error)
            mp->on_bus_error(mp, msg);
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
    int err;
    
    mp->current_playlist = playlist_new();
    if(!mp->current_playlist) {
        err = -errno;
        goto out;
    }
    
    mp->deprecated_playlist = playlist_new();
    if(!mp->deprecated_playlist) {
        err = -errno;
        goto cleanup1;
    }
    
    mp->playbin2 = gst_element_factory_make("playbin2", NULL);
    if(!mp->playbin2) {
        err = -MEDIA_PLAYER_GST_ERROR;
        goto cleanup2;
    }
    
    bus = gst_element_get_bus(mp->playbin2);
    
    gst_bus_add_watch(bus, &bus_watcher, mp);
    
    mp->current_media   = NULL;
    mp->state           = GST_STATE_NULL;
    mp->volume          = 100;
    mp->muted           = false;
    mp->repeat          = false;
    mp->shuffle         = false;
    
    g_object_set(mp->playbin2, "volume", (double) mp->volume / 100.0f, NULL);
    
    return 0;
    
cleanup2:
    playlist_delete(mp->deprecated_playlist);
cleanup1:
    playlist_delete(mp->current_playlist);
out:
    return err;
}

void media_player_destroy(struct media_player *__restrict mp)
{
    media_player_stop(mp);
    
    gst_object_unref(mp->playbin2);
    playlist_delete(mp->current_playlist);
}

const struct media *
media_player_current_media(const struct media_player *__restrict mp)
{
    return mp->current_media;
}

int media_player_play(struct media_player *__restrict mp)
{
    if(mp->state == GST_STATE_PLAYING)
        return 0;
    
    mp->current_media = playlist_first(mp->current_playlist);
    
    if(!mp->current_media)
        return -EINVAL;
    
    return media_player_play_media(mp, mp->current_media);
}

int media_player_play_media(struct media_player *__restrict mp,
                            struct media *m)
{
    int err;
    
    assert(m && "No media passed");
        
    if(!playlist_contains(mp->current_playlist, m->path)) {
        err = playlist_add_media(mp->current_playlist, m);
        if(err < 0)
            return err;
    }
    
    media_player_stop(mp);
    
    g_object_set(mp->playbin2, "uri", m->uri, NULL);
    
    gst_element_set_state(mp->playbin2, GST_STATE_PLAYING);
    
    mp->current_media = m;
    
    return 0;
}

int media_player_play_track(struct media_player *__restrict mp, unsigned int i)
{
    struct media *m;
    
    m = playlist_at(mp->current_playlist, i);
    if(!m)
        return -ENOENT;
    
    return media_player_play_media(mp, m);
}

int media_player_play_file(struct media_player *__restrict mp,
                           const char *__restrict path)
{
    struct media *m;
    
    m = media_new(path);
    if(!m)
        return -errno;
    
    return media_player_play_media(mp, m);
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

void media_player_mute(struct media_player *__restrict mp)
{
    mp->muted = true;
    
    g_object_set(mp->playbin2, "mute", mp->muted, NULL);
}

void media_player_unmute(struct media_player *__restrict mp)
{
    mp->muted = false;
    
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

void media_player_set_repeat(struct media_player *__restrict mp, bool repeat)
{
    mp->repeat = repeat;
}

void media_player_set_shuffle(struct media_player *__restrict mp, bool shuffle)
{
    mp->shuffle = shuffle;
}

int media_player_next(struct media_player *__restrict mp)
{
    struct media *m;
    bool b;
    
    m = playlist_next(mp->current_playlist, mp->current_media);
    if(!m) {
        b = playlist_contains(mp->deprecated_playlist, mp->current_media->path);
        if(b || mp->repeat)
            m = playlist_first(mp->current_playlist);
    }
    
    playlist_clear(mp->deprecated_playlist);
    
    if(!m) {
        mp->current_media = NULL;
        media_player_stop(mp);
        return 0;
    }
    
    return media_player_play_media(mp, m);
}

int media_player_previous(struct media_player *__restrict mp)
{
    struct media *m;
    bool b;
    
    m = playlist_previous(mp->current_playlist, mp->current_media);
    if(!m) {
        b = playlist_contains(mp->deprecated_playlist, mp->current_media->path);
        if(b || mp->repeat)
            m = playlist_last(mp->current_playlist);
    }
    
    playlist_clear(mp->deprecated_playlist);
    
    if(!m) {
        mp->current_media = NULL;
        media_player_stop(mp);
        return 0;
    }
    
    return media_player_play_media(mp, m);
}

int media_player_add_track(struct media_player *__restrict mp, 
                           const char *__restrict path)
{
    return playlist_add_media_path(mp->current_playlist, path);
}

void media_player_remove_track(struct media_player *__restrict mp,
                               const char *__restrict path)
{
    if(!media_is_from_file(mp->current_media, path)) {
        playlist_delete_media_path(mp->current_playlist, path);
        return;
    }

    playlist_take_media(mp->current_playlist, mp->current_media);
    playlist_add_media(mp->deprecated_playlist, mp->current_media);
}

int media_player_add_playlist(struct media_player *__restrict mp,
                              struct playlist *pl)
{
    return playlist_merge(mp->current_playlist, pl);
}

int media_player_set_playlist(struct media_player *__restrict mp,
                              struct playlist *pl)
{
    const char *path;
    int err;
    
    if(!mp->deprecated_playlist) {
        mp->deprecated_playlist = mp->current_playlist;
        mp->current_playlist    = pl;
        
        return 0;
    }
    
    if(mp->current_media) {
        path = mp->current_media->path;
        
        if(playlist_contains(mp->deprecated_playlist, path)) {
            err = playlist_merge(mp->deprecated_playlist, mp->current_playlist);
            if(err < 0)
                return err;
            
            mp->current_playlist = pl;
            return 0;
        }
    }
    
    playlist_delete(mp->deprecated_playlist);
    
    mp->deprecated_playlist = mp->current_playlist;
    mp->current_playlist    = pl;
    
    return 0;
}

bool media_player_empty(const struct media_player *__restrict mp)
{
    return playlist_empty(mp->current_playlist);
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

const struct playlist *
media_player_playlist(const struct media_player *__restrict mp)
{
    return mp->current_playlist;
}

void media_player_on_bus_error(struct media_player *__restrict mp,
                               void (*func)(struct media_player *, GstMessage *))
{
    mp->on_bus_error = func;
}