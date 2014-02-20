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
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <vlc/vlc.h>

#include <libvci/map.h>

#include "media_player.h"

#include "../shared/util.h"


/* use this if a vlclib function fails */
#define MEDIA_PLAYER_VLC_ERROR ECANCELED

static int string_compare(const void *s1, const void *s2)
{
    return strcmp(s1, s2);
}

static unsigned int string_hash(const void *arg)
{
    const char *s;
    unsigned int hval;
    size_t len;
    
    s    = arg;
    hval = 1;
    len  = strlen(s);
    
    while(len--) {
        hval += *s++;
        hval += (hval << 10);
        hval ^= (hval >> 6);
        hval &= 0x0fffffff;
    }
    
    hval += (hval << 3);
    hval ^= (hval >> 11);
    hval += (hval << 15);
    
    return hval;
}

void media_release(void *media)
{
    libvlc_media_release(media);
}

struct media_player *media_player_new(void)
{
    struct media_player *mp;
    int err;
    
    mp = malloc(sizeof(*mp));
    if(!mp)
        return NULL;
    
    err = media_player_init(mp);
    if(err < 0) {
        errno = -err;
        free(mp);
        return NULL;
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
    int err;

    mp->libvlc = libvlc_new(0, NULL);
    if(!mp->libvlc) {
        err = -MEDIA_PLAYER_VLC_ERROR;
        goto out;
    }
    
    mp->player = libvlc_media_player_new(mp->libvlc);
    if(!mp->libvlc) {
        err = -MEDIA_PLAYER_VLC_ERROR;
        goto cleanup1;
    }
    
    mp->list_player = libvlc_media_list_player_new(mp->libvlc);
    if(!mp->list_player) {
        err = -MEDIA_PLAYER_VLC_ERROR;
        goto cleanup2;
    }
    
    mp->media_list = libvlc_media_list_new(mp->libvlc);
    if(!mp->media_list) {
        err = -MEDIA_PLAYER_VLC_ERROR;
        goto cleanup3;
    }
    
    err = map_init(&mp->media_map, 0, &string_compare, &string_hash);
    if(err < 0)
        goto cleanup4;
    
    libvlc_media_list_player_set_media_player(mp->list_player, mp->player);
    libvlc_media_list_player_set_media_list(mp->list_player, mp->media_list);
    
    map_set_data_delete(&mp->media_map, &media_release);
    
    mp->errmsg = NULL;
    
    return 0;

cleanup4:
    libvlc_media_list_release(mp->media_list);
cleanup3:
    libvlc_media_list_player_release(mp->list_player);
cleanup2:
    libvlc_media_player_release(mp->player);
cleanup1:
    libvlc_release(mp->libvlc);
out:
    return err;
}

void media_player_destroy(struct media_player *__restrict mp)
{
    map_destroy(&mp->media_map);
    
    libvlc_media_list_release(mp->media_list);
    libvlc_media_list_player_release(mp->list_player);
    libvlc_media_player_release(mp->player);
    libvlc_release(mp->libvlc);
}

int media_player_add_title(struct media_player *__restrict mp,
                           const char *title)
{
    libvlc_media_t *media;
    int err;

    if(map_contains(&mp->media_map, title))
        return 0;
    
    media = libvlc_media_new_path(mp->libvlc, title);
    if(!media) {
        mp->errmsg = libvlc_errmsg();
        return -MEDIA_PLAYER_VLC_ERROR;
    }
    
    libvlc_media_parse_async(media);
    
    err = map_insert(&mp->media_map, title, media);
    if(err < 0) {
        mp->errmsg = errno_string(-err);
        
        libvlc_media_release(media);
        return err;
    }
    
    libvlc_media_list_lock(mp->media_list);
    err = libvlc_media_list_add_media(mp->media_list, media);
    libvlc_media_list_unlock(mp->media_list);
    
    if(err < 0) {
        mp->errmsg = libvlc_errmsg();
        
        map_take(&mp->media_map, title);
        libvlc_media_release(media);
        return -MEDIA_PLAYER_VLC_ERROR;
    }
    
    return 0;
}

void media_player_remove_title(struct media_player *__restrict mp, 
                              const char *title)
{
    libvlc_media_t *media;
    int index;
    
    media = map_take(&mp->media_map, title);
    if(!media)
        return;
    
    libvlc_media_list_lock(mp->media_list);
    index = libvlc_media_list_index_of_item(mp->media_list, media);
    
    libvlc_media_list_remove_index(mp->media_list, index);
    libvlc_media_list_unlock(mp->media_list);
    
    libvlc_media_release(media);
}

void media_player_play(struct media_player *__restrict mp)
{
    libvlc_media_list_player_play(mp->list_player);    
}

int media_player_play_title(struct media_player *__restrict mp, 
                            const char *title)
{
    libvlc_media_t *media;
    int err;
    
    media = map_retrieve(&mp->media_map, title);
    if(media) {
        libvlc_media_list_player_play_item(mp->list_player, media);
        return 0;
    }

    err = media_player_add_title(mp, title);
    if(err < 0)
        return err;
    
    media = map_retrieve(&mp->media_map, title);
    libvlc_media_list_player_play_item(mp->list_player, media);
    
    return 0;
}

void media_player_stop(struct media_player *__restrict mp)
{
    libvlc_media_list_player_stop(mp->list_player);
}

void media_player_pause(struct media_player *__restrict mp)
{
    libvlc_media_list_player_pause(mp->list_player);
}

void media_player_next_title(struct media_player *__restrict mp)
{
    libvlc_media_list_player_next(mp->list_player);
}

void media_player_previous_title(struct media_player *__restrict mp)
{
    libvlc_media_list_player_previous(mp->list_player);
}

bool media_player_is_playing(struct media_player *__restrict mp)
{
    return libvlc_media_list_player_is_playing(mp->list_player);
}

int media_player_set_volume(struct media_player *__restrict mp, int volume)
{
    int err;
    
    err = libvlc_audio_set_volume(mp->player, volume);
    if(err < 0) {
        mp->errmsg = libvlc_errmsg();
        return -MEDIA_PLAYER_VLC_ERROR;
    }
    
    return 0;
}

void media_player_toggle_mute(struct media_player *__restrict mp)
{
    libvlc_audio_toggle_mute(mp->player);
}

unsigned int media_player_title_count(struct media_player *__restrict mp)
{
    return map_size(&mp->media_map);
}

bool media_player_empty(struct media_player *__restrict mp)
{
    return map_empty(&mp->media_map);
}


int media_player_on_track_finished(struct media_player *__restrict mp,
                   void (*func)(const struct libvlc_event_t *event, void *arg),
                   void *arg)
{
    libvlc_event_manager_t *manager;
    int err;
    
    manager = libvlc_media_player_event_manager(mp->player);
    
    err = libvlc_event_attach(manager, libvlc_MediaPlayerEndReached, func, arg);
    if(err < 0) {
        mp->errmsg = libvlc_errmsg();
        return -MEDIA_PLAYER_VLC_ERROR;
    }
    
    return 0;
}

const char *
media_player_current_track(const struct media_player *__restrict mp)
{
    const char *ret;
    libvlc_media_t *media;
    
    media = libvlc_media_player_get_media(mp->player);
    if(!media)
        return NULL;
    
    if(!libvlc_media_is_parsed(media))
        libvlc_media_parse(media);
    
    ret = libvlc_media_get_meta(media, libvlc_meta_Title);
    
    libvlc_media_release(media);
    
    return ret;
}

const char *media_player_errmsg(const struct media_player *__restrict mp)
{
    return mp->errmsg;
}