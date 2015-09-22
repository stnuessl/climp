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

#ifndef _AUDIO_PLAYER_H_
#define _AUDIO_PLAYER_H_

#include <core/audio-player/gst-engine.h>
#include <core/playlist/playlist.h>

struct audio_player {
    struct gst_engine engine;
    struct playlist playlist;
    
    struct media *active_track;
};

int audio_player_init(struct audio_player *__restrict ap);

void audio_player_destroy(struct audio_player *__restrict ap);

int audio_player_play(struct audio_player *__restrict ap);

int audio_player_pause(struct audio_player *__restrict ap);

int audio_player_stop(struct audio_player *__restrict ap);

int audio_player_play_track(struct audio_player *__restrict ap, int track);

int audio_player_play_next(struct audio_player *__restrict ap);

void audio_player_set_volume(struct audio_player *__restrict ap, 
                             unsigned int vol);

unsigned int audio_player_volume(const struct audio_player *__restrict ap);

void audio_player_set_mute(struct audio_player *__restrict ap, bool mute);

bool audio_player_is_muted(const struct audio_player *__restrict ap);

void audio_player_toggle_mute(struct audio_player *__restrict ap);

bool audio_player_is_playing(const struct audio_player *__restrict ap);

bool audio_player_is_paused(const struct audio_player *__restrict ap);

bool audio_player_is_stopped(const struct audio_player *__restrict ap);

int audio_player_set_stream_position(struct audio_player *__restrict ap, 
                                     unsigned int sec);

int audio_player_stream_position(const struct audio_player *__restrict ap);

struct playlist *audio_player_playlist(struct audio_player *__restrict ap);

const struct media *
audio_player_active_track(const struct audio_player *__restrict ap);



#endif /* _AUDIO_PLAYER_H_ */