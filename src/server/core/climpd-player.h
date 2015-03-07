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

#include "core/climpd-config.h"
#include "core/media-scheduler.h"
#include "core/media.h"
#include "core/playlist.h"

struct climpd_player {
    GstElement *pipeline;
    GstElement *source;
    GstElement *convert;
    GstElement *volume;
    GstElement *sink;
    
    GstDiscoverer *discoverer;
    
    struct media_scheduler *media_scheduler;
    
    GstState state;
    
    unsigned int volume_val;
    
    struct {
        const char *media_active_color;
        const char *media_passive_color;
        unsigned int media_meta_length;
    } config;
};

struct climpd_player *climpd_player_new(struct playlist *__restrict pl,
                                        const struct climpd_config *config);

void climpd_player_delete(struct climpd_player *__restrict cp);

int climpd_player_init(struct climpd_player *__restrict cp,
                       struct playlist *pl,
                       const struct climpd_config *config);
                       
void climpd_player_destroy(struct climpd_player *__restrict cp);

int climpd_player_play_media(struct climpd_player *__restrict cp,
                             struct media *m);

int climpd_player_play_track(struct climpd_player *__restrict cp, 
                             unsigned int index);

int climpd_player_play(struct climpd_player *__restrict cp);

void climpd_player_pause(struct climpd_player *__restrict cp);

void climpd_player_stop(struct climpd_player *__restrict cp);

int climpd_player_next(struct climpd_player *__restrict cp);

int climpd_player_previous(struct climpd_player *__restrict cp);

int climpd_player_peek(struct climpd_player *__restrict cp);

int climpd_player_seek(struct climpd_player *__restrict cp, unsigned int val);

int climpd_player_insert_media(struct climpd_player *__restrict cp, 
                               struct media *m);

struct media *climpd_player_take_index(struct climpd_player *__restrict cp, 
                                       unsigned int index);

void climpd_player_set_volume(struct climpd_player *__restrict cp, 
                              unsigned int vol);

unsigned int climpd_player_volume(const struct climpd_player *__restrict cp);

void climpd_player_set_muted(struct climpd_player *__restrict cp, bool mute);

bool climpd_player_muted(const struct climpd_player *__restrict cp);

void climpd_player_set_repeat(struct climpd_player *__restrict cp, bool repeat);

bool climpd_player_repeat(const struct climpd_player *__restrict cp);

void climpd_player_set_shuffle(struct climpd_player *__restrict cp, 
                               bool shuffle);

bool climpd_player_shuffle(const struct climpd_player *__restrict cp);

bool climpd_player_playing(const struct climpd_player *__restrict cp);

bool climpd_player_paused(const struct climpd_player *__restrict cp);

bool climpd_player_stopped(const struct climpd_player *__restrict cp);

struct media *climpd_player_running(const struct climpd_player *__restrict cp);

int climpd_player_set_playlist(struct climpd_player *__restrict cp,
                               struct playlist *pl);

void climpd_player_print_playlist(struct climpd_player *__restrict cp, int fd);

void climpd_player_print_files(struct climpd_player *__restrict cp, int fd);

void climpd_player_print_running_track(struct climpd_player *__restrict cp, 
                                       int fd);

void climpd_player_print_volume(struct climpd_player *__restrict cp, int fd);

void climpd_player_print_state(const struct climpd_player *__restrict cp, 
                               int fd);

#endif /* _CLIMPD_PLAYER_H_ */