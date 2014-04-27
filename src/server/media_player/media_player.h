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
    GstElement *playbin2;
    GstState state;

    unsigned int volume;
    bool muted;
    
    void (*on_end_of_stream)(struct media_player *);
    void (*on_bus_error)(struct media_player *, GstMessage *);
};

struct media_player *media_player_new(void);

void media_player_delete(struct media_player *__restrict mp);

int media_player_init(struct media_player *__restrict mp);

void media_player_destroy(struct media_player *__restrict mp);

int media_player_play_media(struct media_player *__restrict mp,
                            struct media *m);

void media_player_pause(struct media_player *__restrict mp);

void media_player_stop(struct media_player *__restrict mp);

void media_player_set_muted(struct media_player *__restrict mp, bool muted);

bool media_player_muted(const struct media_player *__restrict mp);

void media_player_set_volume(struct media_player *__restrict mp,
                             unsigned int volume);

unsigned int media_player_volume(const struct media_player *__restrict mp);

bool media_player_playing(const struct media_player *__restrict mp);

bool media_player_paused(const struct media_player *__restrict mp);

bool media_player_stopped(const struct media_player *__restrict mp);

void media_player_on_bus_error(struct media_player *__restrict mp,
                             void (*func)(struct media_player *, GstMessage *));

#endif /* _MEDIA_PLAYER_H_ */