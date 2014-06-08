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
static GstElement *playbin2;
static struct media_scheduler *sched;
static struct playlist *playlist;
static GstState state;
static unsigned int volume;
static bool muted;
static void (*on_bus_error)(GstMessage *);

extern struct climpd_config conf;

static void climpd_player_play_uri(const char *uri)
{
    climpd_player_stop();
    
    g_object_set(playbin2, "uri", uri, NULL);
    
    state = GST_STATE_PLAYING;
    
    gst_element_set_state(playbin2, state);
}

static void on_end_of_stream(void)
{
    const struct media *m;
    
    m = media_scheduler_running(sched);
    if(m)
        climpd_log_i(tag, "finished playing '%s'\n", media_info(m)->title);
    
    m = media_scheduler_next(sched);
        if(!m) {
        climpd_log_i(tag, "finished playlist\n");
        return;
    }
    
    climpd_player_play_uri(media_uri(m));
}

static gboolean bus_watcher(GstBus *bus, GstMessage *msg, gpointer data)
{
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            if(on_bus_error)
                on_bus_error(msg);
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
    int err;
    
    if(!media_is_parsed(m)) {
        err = media_manager_parse_media(m);
        if(err < 0) {
            dprintf(fd, "climpd: failed to parse %s\n", media_path(m));
            return;
        }
    }
    
    i = media_info(m);
    
    dprintf(fd, "%s ( %3u )\t%-*.*s %-*.*s %-*.*s\n" COLOR_CODE_DEFAULT,
            color, index, 
            conf.media_meta_length, conf.media_meta_length, i->title, 
            conf.media_meta_length, conf.media_meta_length, i->artist, 
            conf.media_meta_length, conf.media_meta_length, i->album);
}

int climpd_player_init(struct playlist *pl, bool repeat, bool shuffle)
{
    GstBus *bus;
    int err;
    
    playbin2 = gst_element_factory_make("playbin2", NULL);
    if(!playbin2)
        return -ENOMEM;
    
    bus = gst_element_get_bus(playbin2);
    
    gst_bus_add_watch(bus, &bus_watcher, NULL);
    
    state  = GST_STATE_NULL;
    volume = 100;
    muted  = false;
    
    g_object_set(playbin2, "volume", (double) volume / 100.0f, NULL);

    sched = media_scheduler_new(pl);
    if(!sched) {
        err = -errno;
        goto cleanup1;
    }
    
    media_scheduler_set_repeat(sched, repeat);
    media_scheduler_set_shuffle(sched, shuffle);
    
    
    playlist = pl;

    climpd_log_i(tag, "initialization complete\n");
    
    return 0;

cleanup1:
    gst_object_unref(playbin2);
    
    return err;
}

void climpd_player_destroy(void)
{
    media_scheduler_delete(sched);
    gst_object_unref(playbin2);
    
    climpd_log_i(tag, "destroyed\n");
}

int climpd_player_play_media(struct media *m)
{
    int err;
    
    err = media_scheduler_set_running_media(sched, m);
    if(err < 0)
        return err;
    
    climpd_player_play_uri(media_uri(m));
    
    return 0;
}

int climpd_player_play_track(unsigned int index)
{
    struct media *m;
    int err;
    
    index -= 1;
    
    if(index >= playlist_size(playlist))
        return -EINVAL;
    
    err = media_scheduler_set_running_track(sched, index);
    if(err < 0)
        return err;
    
    m = playlist_at(playlist, index);
    
    climpd_player_play_uri(media_uri(m));

    return 0;
}

int climpd_player_play(void)
{
    struct media *m;
    
    if(climpd_player_playing())
        return 0;
    
    m = media_scheduler_next(sched);
    if(!m)
        return -ENOENT;
    
    climpd_player_play_uri(media_uri(m));
    
    return 0;
}

void climpd_player_pause(void)
{
    if(state == GST_STATE_PAUSED)
        return;
    
    state = GST_STATE_PAUSED;
    
    gst_element_set_state(playbin2, state);
}

void climpd_player_stop(void)
{
    if(state == GST_STATE_NULL)
        return;
    
    state = GST_STATE_NULL;
    
    gst_element_set_state(playbin2, state);
}

int climpd_player_next(void)
{
    const struct media *m;
    
    if(playlist_empty(playlist))
        return -ENOENT;
    
    m = media_scheduler_next(sched);
    if(!m) {
        climpd_player_stop();
        return 0;
    }
    
    climpd_player_play_uri(media_uri(m));
    
    return 0;
}

int climpd_player_previous(void)
{
    const struct media *m;

    if(playlist_empty(playlist))
        return -ENOENT;
    
    m = media_scheduler_previous(sched);
    if(!m) {
        climpd_player_stop();
        return 0;
    }
    
    climpd_player_play_uri(media_uri(m));
    
    return 0;
}

int climpd_player_add_media(struct media *m)
{
    return media_scheduler_insert_media(sched, m);
}

void climpd_player_take_media(struct media *m)
{
    media_scheduler_take_media(sched, m);
}

void climpd_player_set_volume(unsigned int vol)
{
    vol = max(vol, 0);
    vol = min(vol, 120);
    
    volume = vol;
    
    g_object_set(playbin2, "volume", (double) volume / 100.0f, NULL);
}

unsigned int climpd_player_volume(void)
{
    return volume;
}

void climpd_player_set_muted(bool m)
{
    muted = m;
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
    
    err = media_scheduler_set_playlist(sched, pl);
    if(err < 0)
        return err;
    
    playlist = pl;
    
    return 0;
}

void climpd_player_print_playlist(int fd)
{
    struct media **m, *current;
    unsigned int index;
    
    current = media_scheduler_running(sched);
    index   = 0;
    
    playlist_for_each(playlist, m) {
        index += 1;
        
        if(*m == current)
            print_media(fd, index, *m, conf.media_active_color);
        else
            print_media(fd, index, *m, conf.media_passive_color);
    }
}

void climpd_player_print_files(int fd)
{
    const struct media **m;
    
    playlist_for_each(playlist, m)
        dprintf(fd, "%s\n", (*m)->path);
}

void climpd_player_print_current_track(int fd)
{
    struct media *m;
    unsigned int index;
    
    m =  media_scheduler_running(sched);
    if(!m) {
        dprintf(fd, "No current track available\n");
        return;
    }
    
    index = playlist_index_of(playlist, m);
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