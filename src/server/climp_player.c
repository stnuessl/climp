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

static void on_end_of_stream(struct player *mp)
{
    struct climp_player *cp;
    const struct media *m;
    
    cp = container_of(mp, struct climp_player, mp);
    
    if(!playlist_iterator_has_next(cp->pl))
        return;
    
    m = playlist_iterator_next(cp->it);
    
    media_player_play_media(mp, m);
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
    
    cp->it = playlist_iterator(cp->pl);
    
    return 0;
    
cleanup1:
    media_player_destroy(&cp->mp);
out:
    return err;
}

void climp_player_destroy(struct climp_player *__restrict cp)
{
    playlist_delete(cp->pl);
    media_player_delete(&cp->mp);
}

const struct playlist *
climp_player_playlist(const struct climp_player *__restrict cp)
{
    return cp->pl;
}

int climp_player_play_file(struct climp_player *__restrict cp, 
                           const char *__restrict path)
{
    struct media *m;
    int err;
    
    m = playlist_retrieve_path(cp->pl, path);
    if(!m) {
        m = media_new(path);
        if(!m)
            return -errno;
        
        err = playlist_insert_media(cp->pl, m);
        if(err < 0) {
            media_delete(m);
            return err;
        }
    }

    playlist_iterator_set_current(cp->it, m);
    media_player_play_media(&cp->mp, m);
    
    return 0;
}

int climp_player_play_media(struct climp_player *__restrict cp, 
                            struct media *m)
{
    int err;
    
    if(!playlist_contains(cp->pl, m)) {
        err = playlist_insert_media(cp->pl, m);
        if(err < 0)
            return err;
    }
    
    playlist_iterator_set_current(cp->it, m);
    media_player_play_media(&cp->mp, m);
    
    return 0;
}

int climp_player_play_track(struct climp_player *__restrict cp, 
                            unsigned int index)
{
    /* TODO: implement me */
    return 0;
}

int climp_player_play(struct climp_player *__restrict cp)
{
    const struct media *m;
    
    if(!playlist_iterator_has_next(cp->it))
        return -ENOENT;
    
    m = playlist_iterator_next(cp->it);
    
    media_player_play_media(&cp->mp, m);
    
    return 0;
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
    return climp_player_play(cp);
}

int climp_player_previous(struct climp_player *__restrict cp)
{
    const struct media *m;
    
    if(!playlist_iterator_has_previous(cp->it))
        return -ENOENT;
    
    m = playlist_iterator_previous(cp->it);
    
    media_player_play_media(&cp->mp, m);
    
    return 0;
}

int climp_player_add_file(struct climp_player *__restrict cp, const char *path)
{
    return playlist_insert_path(cp->pl, path);
}

int climp_player_add_media(struct climp_player *__restrict cp, struct media *m)
{
    return playlist_insert_media(cp->pl, m);
}

void climp_player_delete_file(struct climp_player *__restrict cp,
                              const char *path)
{
    playlist_delete_path(cp->pl, path);
}

void climp_player_delete_media(struct climp_player *__restrict cp,
                               struct media *m)
{
    playlist_delete_media(cp->pl, m);
}

void climp_player_take_media(struct climp_player *__restrict cp,
                             struct media *m)
{
    playlist_take_media(cp->pl, m);
}

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
    playlist_iterator_set_repeat(cp->it, repeat);
}

bool climp_player_repeat(const struct climp_player *__restrict cp)
{
    return playlist_iterator_repeat(cp->it);
}

void climp_player_set_shuffle(struct climp_player *__restrict cp, bool shuffle)
{
    playlist_iterator_set_shuffle(cp->it, shuffle);
}

bool climp_player_shuffle(const struct climp_player *__restrict cp)
{
    return playlist_iterator_shuffle(cp->it);
}

void climp_player_set_playlist(struct climp_player *__restrict cp, 
                               struct playlist *pl)
{
    playlist_delete(cp->pl);
    cp->pl = pl;
}