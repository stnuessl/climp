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
#include <stdlib.h>
#include <errno.h>

#include "media_player.h"

struct media_player *media_player_new(void)
{
    struct media_player *__restrict mp;
    int err;
    
    mp = malloc(sizeof(*mp));
    if(!mp)
        return NULL;
    
    err = media_player_init(mp);
    if(err < 0) {
        free(mp);
        return err;
    }
    
    return mp;
}

void media_player_delete(struct media_player *__restrict mp)
{
    media_player_destroy(mp);
    free(mp);
}

int media_player_init(struct media_player *__restrict mp)
{
    return 0;
}

void media_player_destroy(struct media_player *__restrict mp)
{
    
}

const struct media *
media_player_current_media(struct media_player *__restrict mp)
{
    return mp->current_media;
}

struct playlist *media_player_playlist(struct media_player *__restrict mp)
{
    return mp->playlist;
}

void media_player_play(struct media_player *__restrict mp)
{
    
}

void media_player_pause(struct media_player *__restrict mp)
{
    
}

void media_player_stop(struct media_player *__restrict mp)
{
    
}

void media_player_toggle_mute(struct media_player *__restrict mp)
{
    
}

void media_player_set_volume(struct media_player *__restrict mp,
                             unsigned int volume)
{
    
}

unsigned int media_player_volume(const struct media_player *__restrict mp)
{
    return mp->volume;
}