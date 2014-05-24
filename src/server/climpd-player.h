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

#include "media-player/playlist.h"
#include "media-player/media.h"

int climpd_player_init(struct playlist *pl, bool repeat, bool shuffle);

void climpd_player_destroy(void);

int climpd_player_play_file(const char *__restrict path);

int climpd_player_play_media(struct media *m);

int climpd_player_play_track(unsigned int index);

int climpd_player_play(void);

void climpd_player_pause(void);

void climpd_player_stop(void);

int climpd_player_next(void);

int climpd_player_previous(void);

int climpd_player_add_file(const char *path);

int climpd_player_add_media(struct media *m);

void climpd_player_delete_file(const char *path);

void climpd_player_delete_media(struct media *m);

void climpd_player_take_media(struct media *m);

void climpd_player_set_volume(unsigned int volume);

unsigned int climpd_player_volume(void);

void climpd_player_set_muted(bool muted);

bool climpd_player_muted(void);

void climpd_player_set_repeat(bool repeat);

bool climpd_player_repeat(void);

void climpd_player_set_shuffle(bool shuffle);

bool climpd_player_shuffle(void);

bool climpd_player_playing(void);

bool climpd_player_paused(void);

bool climpd_player_stopped(void);

const struct media *climpd_player_current(void);

void climpd_player_set_playlist(struct playlist *pl);

void climpd_player_print_playlist(int fd);

void climpd_player_print_files(int fd);

void climpd_player_print_current_track(int fd);

void climpd_player_print_volume(int fd);

void climpd_player_print_state(int fd);

#endif /* _CLIMPD_PLAYER_H_ */