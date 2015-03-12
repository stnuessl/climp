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

#ifndef _CLIMPD_PLAYER_H_
#define _CLIMPD_PLAYER_H_

#include <stdbool.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/stack.h>

#include <core/climpd-config.h>
#include <core/playlist.h>
#include <core/media-list.h>
#include <core/media.h>

enum climpd_player_state {
    CLIMPD_PLAYER_PLAYING = GST_STATE_PLAYING,
    CLIMPD_PLAYER_PAUSED  = GST_STATE_PAUSED,
    CLIMPD_PLAYER_STOPPED = GST_STATE_NULL,
};

struct climpd_player {
    GstElement *pipeline;
    GstElement *source;
    GstElement *convert;
    GstElement *volume;
    GstElement *sink;
    
    GstDiscoverer *discoverer;
    
    GstState state;
    unsigned int volume_val;
    
    struct playlist playlist;
    struct media *running_track;
    
    struct {
        const char *media_active_color;
        const char *media_passive_color;
        unsigned int media_meta_length;
    } config;
};

int climpd_player_init(struct climpd_player *__restrict cp,
                       const struct climpd_config *config);

void climpd_player_destroy(struct climpd_player *__restrict cp);

int climpd_player_play_path(struct climpd_player *__restrict cp,
                            const char *path);

int climpd_player_play_track(struct climpd_player *__restrict cp, int index);

int climpd_player_play(struct climpd_player *__restrict cp);

void climpd_player_pause(struct climpd_player *__restrict cp);

void climpd_player_stop(struct climpd_player *__restrict cp);

int climpd_player_next(struct climpd_player *__restrict cp);

int climpd_player_previous(struct climpd_player *__restrict cp);

int climpd_player_peek(struct climpd_player *__restrict cp);

int climpd_player_seek(struct climpd_player *__restrict cp, unsigned int val);

int climpd_player_insert(struct climpd_player *__restrict cp, 
                         const char *__restrict path);

void climpd_player_delete_index(struct climpd_player *__restrict cp, int index);

void climpd_player_delete_media(struct climpd_player *__restrict cp,
                                const struct media *__restrict m);

void climpd_player_set_volume(struct climpd_player *__restrict cp, 
                              unsigned int vol);

unsigned int climpd_player_volume(const struct climpd_player *__restrict cp);

void climpd_player_set_muted(struct climpd_player *__restrict cp, bool mute);

bool climpd_player_muted(const struct climpd_player *__restrict cp);

bool climpd_player_toggle_muted(struct climpd_player *__restrict cp);

void climpd_player_set_repeat(struct climpd_player *__restrict cp, bool repeat);

bool climpd_player_repeat(const struct climpd_player *__restrict cp);

bool climpd_player_toggle_repeat(struct climpd_player *__restrict cp);

void climpd_player_set_shuffle(struct climpd_player *__restrict cp, 
                               bool shuffle);

bool climpd_player_shuffle(const struct climpd_player *__restrict cp);

bool climpd_player_toggle_shuffle(struct climpd_player *__restrict cp);

enum climpd_player_state 
climpd_player_state(const struct climpd_player *__restrict cp);

bool climpd_player_is_playing(const struct climpd_player *__restrict cp);

bool climpd_player_is_paused(const struct climpd_player *__restrict cp);

bool climpd_player_is_stopped(const struct climpd_player *__restrict cp);

int climpd_player_add_media_list(struct climpd_player *__restrict cp,
                                 struct media_list *__restrict ml);

int climpd_player_set_media_list(struct climpd_player *__restrict cp,
                                 struct media_list *__restrict ml);

void climpd_player_remove_media_list(struct climpd_player *__restrict cp,
                                     struct media_list *__restrict ml);

void climpd_player_clear_playlist(struct climpd_player *__restrict cp);

void climpd_player_set_config(struct climpd_player *__restrict cp,
                              const struct climpd_config *__restrict conf);

void climpd_player_print_playlist(struct climpd_player *__restrict cp, int fd);

void climpd_player_print_files(struct climpd_player *__restrict cp, int fd);

void climpd_player_print_running_track(struct climpd_player *__restrict cp, 
                                       int fd);

void climpd_player_print_volume(struct climpd_player *__restrict cp, int fd);

#endif /* _CLIMPD_PLAYER_H_ */