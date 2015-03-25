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

#include <obj/media-list.h>
#include <obj/media.h>

enum climpd_player_state {
    CLIMPD_PLAYER_PLAYING = GST_STATE_PLAYING,
    CLIMPD_PLAYER_PAUSED  = GST_STATE_PAUSED,
    CLIMPD_PLAYER_STOPPED = GST_STATE_NULL,
};

void climpd_player_init(void);

void climpd_player_destroy(void);

int climpd_player_play_path(const char *path);

int climpd_player_play_track(int index);

int climpd_player_play(void);

void climpd_player_pause(void);

void climpd_player_stop(void);

int climpd_player_next(void);

int climpd_player_previous(void);

int climpd_player_peek(void);

int climpd_player_seek(unsigned int val);

int climpd_player_insert(const char *__restrict path);

void climpd_player_delete_index(int index);

void climpd_player_delete_media(const struct media *__restrict m);

void climpd_player_set_volume(unsigned int vol);

unsigned int climpd_player_volume(void);

void climpd_player_set_muted(bool mute);

bool climpd_player_muted(void);

bool climpd_player_toggle_muted(void);

void climpd_player_set_repeat(bool repeat);

bool climpd_player_repeat(void);

bool climpd_player_toggle_repeat(void);

void climpd_player_set_shuffle(bool shuffle);

bool climpd_player_shuffle(void);

bool climpd_player_toggle_shuffle(void);

enum climpd_player_state climpd_player_state(void);

bool climpd_player_is_playing(void);

bool climpd_player_is_paused(void);

bool climpd_player_is_stopped(void);

int climpd_player_add_media_list(struct media_list *__restrict ml);

int climpd_player_set_media_list(struct media_list *__restrict ml);

void climpd_player_remove_media_list(struct media_list *__restrict ml);

void climpd_player_clear_playlist(void);

void climpd_player_print_current_track(int fd);

void climpd_player_print_playlist(int fd);

void climpd_player_print_files(int fd);

void climpd_player_print_volume(int fd);


#endif /* _CLIMPD_PLAYER_H_ */