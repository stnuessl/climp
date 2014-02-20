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

#include <vlc/vlc.h>

#include <libvci/map.h>

struct media_player {
    libvlc_instance_t *libvlc;
    libvlc_media_list_player_t *list_player;
    libvlc_media_player_t *player;
    libvlc_media_list_t *media_list;
    
    struct map media_map;

    const char *errmsg;
};

struct media_player *media_player_new(void);

void media_player_delete(struct media_player *__restrict mp);

int media_player_init(struct media_player *__restrict mp);

void media_player_destroy(struct media_player *__restrict mp);

int media_player_add_title(struct media_player *__restrict mp, 
                           const char *title);

void media_player_remove_title(struct media_player *__restrict mp, 
                               const char *title);

void media_player_play(struct media_player *__restrict mp);

int media_player_play_title(struct media_player *__restrict mp, 
                             const char *title);

void media_player_stop(struct media_player *__restrict mp);

void media_player_pause(struct media_player *__restrict mp);

void media_player_next_title(struct media_player *__restrict mp);

void media_player_previous_title(struct media_player *__restrict mp);

bool media_player_is_playing(struct media_player *__restrict mp);

int media_player_set_volume(struct media_player *__restrict mp, int volume);

void media_player_toggle_mute(struct media_player *__restrict mp);

unsigned int media_player_title_count(struct media_player *__restrict mp);

bool media_player_empty(struct media_player *__restrict mp);

int media_player_on_track_finished(struct media_player *__restrict mp,
                    void (*func)(const struct libvlc_event_t *event, void *arg),
                    void *arg);

const char *
media_player_current_track(const struct media_player *__restrict mp);

const char *media_player_errmsg(const struct media_player *__restrict mp);

#endif /* _MEDIA_PLAYER_H_ */