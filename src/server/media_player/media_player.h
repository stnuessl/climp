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

#ifndef _MEDIA_PLAYER_H_
#define _MEDIA_PLAYER_H_

#include <stdbool.h>

#include <gst/gst.h>

#include "playlist.h"
#include "media.h"

struct media_player {
    struct playlist *current_playlist;
    struct playlist *deprecated_playlist;
    struct media *current_media;

    GstElement *playbin2;
    
    GstState state;

    void (*on_bus_error)(struct media_player *, GstMessage *);

    unsigned int volume;
    bool muted;
    bool repeat;
    bool shuffle;
};

struct media_player *media_player_new(void);

void media_player_delete(struct media_player *__restrict mp);

int media_player_init(struct media_player *__restrict mp);

void media_player_destroy(struct media_player *__restrict mp);

const struct media *
media_player_current_media(const struct media_player *__restrict mp);

int media_player_play(struct media_player *__restrict mp);

int media_player_play_media(struct media_player *__restrict mp,
                            struct media *m);

int media_player_play_track(struct media_player *__restrict mp, unsigned int i);

void media_player_pause(struct media_player *__restrict mp);

void media_player_stop(struct media_player *__restrict mp);

void media_player_mute(struct media_player *__restrict mp);

void media_player_unmute(struct media_player *__restrict mp);

void media_player_set_volume(struct media_player *__restrict mp,
                             unsigned int volume);

unsigned int media_player_volume(const struct media_player *__restrict mp);

void media_player_set_repeat(struct media_player *__restrict mp, bool repeat);

void media_player_set_shuffle(struct media_player *__restrict mp, bool shuffle);

int media_player_next(struct media_player *__restrict mp);

int media_player_previous(struct media_player *__restrict mp);

int media_player_add_track(struct media_player *__restrict mp, 
                           const char *__restrict path);

void media_player_remove_track(struct media_player *__restrict mp,
                               const char *__restrict path);

int media_player_add_playlist(struct media_player *__restrict mp,
                              struct playlist *pl);

int media_player_set_playlist(struct media_player *__restrict mp,
                               struct playlist *pl);

bool media_player_empty(const struct media_player *__restrict mp);

bool media_player_playing(const struct media_player *__restrict mp);

bool media_player_paused(const struct media_player *__restrict mp);

bool media_player_stopped(const struct media_player *__restrict mp);

const struct playlist *
media_player_playlist(const struct media_player *__restrict mp);

void media_player_on_bus_error(struct media_player *__restrict mp,
                             void (*func)(struct media_player *, GstMessage *));

#endif /* _MEDIA_PLAYER_H_ */