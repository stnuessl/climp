/*
 * Copyright (C) 2015  Steffen Nüssle
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

#include <gst/gst.h>

enum media_player_state {
    MEDIA_PLAYER_RUNNING = GST_STATE_PLAYING,
    MEDIA_PLAYER_PAUSED  = GST_STATE_PAUSED,
    MEDIA_PLAYER_STOPPED = GST_STATE_NULL,
};

void media_player_init(void (*on_eos)(void));

void media_player_destroy(void);

void media_player_play(const char *__restrict uri);

void media_player_pause(void);

void media_player_stop(void);

int media_player_peek(void);

int media_player_seek(unsigned int sec);

void media_player_set_volume(unsigned int vol);

unsigned int media_player_volume(void);

void media_player_set_muted(bool muted);

bool media_player_muted(void);


#endif /* _MEDIA_PLAYER_H_ */