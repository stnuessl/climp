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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <libvci/macro.h>

#include "climp_player.h"

static void on_end_of_stream(struct media_player *mp)
{
    struct climp_player *cp;
    
    cp = container_of(mp, struct climp_player, mp);
    
    if(!cp->next)
        return;
    
    media_player_play_media(mp, cp->next);
    
    cp->next = playlist_next(cp->pl, cp->next);
}

struct climp_player *climp_player_new(void)
{
    struct climp_player *cp;
    int err;
    
    cp = malloc(sizeof(*cp));
    if(!cp)
        return NULL;
    
    err = climp_player_init(cp);
    if(err < 0) {
        free(cp);
        return NULL;
    }
    
    return cp;
}

void climp_player_delete(struct climp_player *__restrict cp)
{
    climp_player_destroy(cp);
    free(cp);
}

int climp_player_init(struct climp_player *__restrict cp)
{
    int err;
    
    err = media_player_init(&cp->mp);
    if(err < 0)
        goto out;
    
    media_player_on_end_of_stream(&cp->mp, &on_end_of_stream);
    
    mp->pl = playlist_new();
    if(!mp->pl) {
        err = -errno;
        goto cleanup1;
    }
    
    err = random_init(&cp->rand);
    if(err < 0)
        goto cleanup2;
    
    cp->next = NULL;
    
    cp->repeat  = false;
    cp->shuffle = false;
    
    return 0;
    
cleanup2:
    playlist_delete(cp->pl);
cleanup1:
    media_player_destroy(&cp->mp);
out:
    return err;
}

void climp_player_destroy(struct climp_player *__restrict cp)
{
    random_destroy(&cp->rand);
    playlist_delete(cp->pl);
    media_player_delete(&cp->mp);
}

int climp_player_play_file(struct climp_player *__restrict cp, 
                           const char *__restrict path)
{
    
}

int climp_player_play_media(struct climp_player *__restrict cp, 
                            struct media *m)
{
    
}

int climp_player_play(struct climp_player *__restrict cp)
{
    
}

void climp_player_pause(struct climp_player *__restrict cp)
{
    media_player_pause(&cp->mp);
}

void climp_player_stop(struct climp_player *__restrict cp)
{
    media_player_stop(&cp->mp);
}

int climp_player_next(struct climp_player *__restrict cp)
{
    
}

int climp_player_previous(struct climp_player *__restrict cp)
{
    
}

int climp_player_add_file(struct climp_player *__restrict cp, const char *path)
{
    return playlist_add_media_path(cp->pl, path);
}

int climp_player_add_media(struct climp_player *__restrict cp, struct media *m)
{
    return playlist_add_media(cp->pl, m);
}

void climp_player_delete_file(struct climp_player *__restrict cp,
                              const char *path);

void climp_player_delete_media(struct climp_player *__restrict cp,
                               struct media *m);

void climp_player_remove_media(struct climp_player *__restrict cp,
                               const struct media *m);

void climp_player_set_volume(struct climp_player *__restrict cp, 
                             unsigned int volume)
{
    volume = max(volume, MEDIA_PLAYER_MIN_VOLUME);
    volume = min(volume, MEDIA_PLAYER_MAX_VOLUME);
    
    media_player_set_volume(&cp->mp, volume);
}

unsigned int climp_player_volume(const struct climp_player *__restrict cp)
{
    return media_player_volume(&cp->mp);
}

void climp_player_set_muted(struct climp_player *__restrict cp, bool muted)
{
    media_player_set_muted(&cp->mp, muted);
}

bool climp_player_muted(const struct climp_player *__restrict cp)
{
    return media_player_muted(&cp->mp);
}

void climp_player_set_repeat(struct climp_player *__restrict cp, bool repeat)
{
    cp->repeat = repeat;
}

bool climp_player_repeat(const struct climp_player *__restrict cp)
{
    return cp->repeat;
}

void climp_player_set_shuffle(struct climp_player *__restrict cp, bool shuffle)
{
    cp->shuffle = shuffle;
}

bool climp_player_shuffle(const struct climp_player *__restrict cp)
{
    return cp->shuffle;
}