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

#ifndef _CLIMP_PLAYER_H_
#define _CLIMP_PLAYER_H_

#include "media_player/media_player.h"
#include "media_player/playlist.h"
#include "media_player/media.h"

struct climp_player {
    struct media_player mp;
    struct playlist *pl;
};

struct climp_player *climp_player_new(void);

void climp_player_delete(struct climp_player *__restrict cp);

int climp_player_init(struct climp_player *__restrict cp);

void climp_player_destroy(struct climp_player *__restrict cp);

int climp_player_play_file(struct climp_player *__restrict cp, 
                           const char *__restrict path);

void climp_player_play_media(struct climp_player *__restrict cp, 
                             struct media *m);

int climp_player_play_track(struct climp_player *__restrict cp, 
                            unsigned int index);

int climp_player_play(struct climp_player *__restrict cp);

void climp_player_pause(struct climp_player *__restrict cp);

void climp_player_stop(struct climp_player *__restrict cp);

int climp_player_next(struct climp_player *__restrict cp);

int climp_player_previous(struct climp_player *__restrict cp);

int climp_player_add_file(struct climp_player *__restrict cp, const char *path);

int climp_player_add_media(struct climp_player *__restrict cp, struct media *m);

void climp_player_delete_file(struct climp_player *__restrict cp,
                              const char *path);

void climp_player_delete_media(struct climp_player *__restrict cp,
                               struct media *m);

void climp_player_take_media(struct climp_player *__restrict cp,
                             struct media *m);

void climp_player_set_volume(struct climp_player *__restrict cp, 
                             unsigned int volume);

unsigned int climp_player_volume(const struct climp_player *__restrict cp);

void climp_player_set_muted(struct climp_player *__restrict cp, bool muted);

bool climp_player_muted(const struct climp_player *__restrict cp);

void climp_player_set_repeat(struct climp_player *__restrict cp, bool repeat);

bool climp_player_repeat(const struct climp_player *__restrict cp);

void climp_player_set_shuffle(struct climp_player *__restrict cp, bool shuffle);

bool climp_player_shuffle(const struct climp_player *__restrict cp);

void climp_player_set_playlist(struct climp_player *__restrict cp, 
                               struct playlist *pl);

const struct playlist *
climp_player_playlist(const struct climp_player *__restrict cp);

#endif /* _CLIMP_PLAYER_H_ */