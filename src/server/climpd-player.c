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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <libvci/macro.h>
#include <libvci/error.h>

#include "media-player/playlist.h"
#include "media-player/media-player.h"
#include "climpd-player.h"
#include "climpd-log.h"
#include "climpd-config.h"
#include "terminal-color-map.h"

static const char *tag = "climpd-player";
static struct media_player media_player;
static struct playlist *playlist;

extern struct climpd_config conf;

static void on_end_of_stream(struct media_player *mp)
{
    const struct media *m;
    const struct media_info *i;
    
    i = media_info(playlist_current(playlist));
    
    climpd_log_i(tag, "finished playing '%s'\n", i->title);

    m = playlist_next(playlist);
    if(!m) {
        climpd_log_i(tag, "finished playlist\n");
        return;
    }
    
    media_player_play_media(mp, m);
}

static void print_media(int fd, 
                        unsigned int index, 
                        const struct media *__restrict m,
                        const char *color)
{
    const struct media_info *i;
    
    i = media_info(m);
    
    dprintf(fd, "%s ( %3u )\t%-*.*s %-*.*s %-*.*s\n" COLOR_CODE_DEFAULT,
            color, index, 
            conf.media_meta_length, conf.media_meta_length, i->title, 
            conf.media_meta_length, conf.media_meta_length, i->artist, 
            conf.media_meta_length, conf.media_meta_length, i->album);
}

int climpd_player_init(struct playlist *pl, bool repeat, bool shuffle)
{
    int err;
    
    err = media_player_init(&media_player);
    if(err < 0) {
        climpd_log_e(tag, "media_player_init(): %s\n", strerr(-err));
        return err;
    }
    
    media_player_on_end_of_stream(&media_player, &on_end_of_stream);
    
    playlist = pl;
    
    playlist_set_repeat(playlist, repeat);
    playlist_set_shuffle(playlist, shuffle);
    
    climpd_log_i(tag, "initialization complete\n");
    
    return 0;
}

void climpd_player_destroy(void)
{
    media_player_delete(&media_player);
    
    climpd_log_i(tag, "destroyed\n");
}

int climpd_player_play_file(const char *__restrict path)
{
    struct media *m;
    int err;
    
    m = playlist_retrieve_path(playlist, path);
    if(!m) {
        m = media_new(path);
        if(!m)
            return -errno;
        
        err = climpd_player_play_media(m);
        if(err < 0) {
            media_delete(m);
            return err;
        }
    }
    
    return climpd_player_play_media(m);
}

int climpd_player_play_media(struct media *m)
{
    int err;
    
    err = playlist_set_current(playlist, m);
    if(err < 0)
        return err;
    
    media_player_play_media(&media_player, m);
    
    return 0;
}

int climpd_player_play_track(unsigned int index)
{
    struct media *m;
    int err;
    
    if(index >= playlist_size(playlist))
        return -EINVAL;
    
    m = playlist_at(playlist, index - 1);
    
    err = playlist_set_current(playlist, m);
    if(err < 0)
        return err;
    
    media_player_play_media(&media_player, m);
    
    return 0;
}

int climpd_player_play(void)
{
    if(media_player_playing(&media_player))
        return 0;
    
    return climpd_player_next();
}

void climpd_player_pause(void)
{
    media_player_pause(&media_player);
}

void climpd_player_stop(void)
{
    media_player_stop(&media_player);
}

int climpd_player_next(void)
{
    const struct media *m;
    
    if(playlist_empty(playlist))
        return -ENOENT;
    
    m = playlist_next(playlist);
    if(!m)
        return 0;
    
    media_player_play_media(&media_player, m);
    
    return 0;
}

int climpd_player_previous(void)
{
    const struct media *m;

    if(playlist_empty(playlist))
        return -ENOENT;
    
    m = playlist_previous(playlist);
    if(!m)
        return 0;
    
    media_player_play_media(&media_player, m);
    
    return 0;
}

int climpd_player_add_file(const char *path)
{
    return playlist_insert_path(playlist, path);
}

int climpd_player_add_media(struct media *m)
{
    return playlist_insert_media(playlist, m);
}

void climpd_player_delete_file(const char *path)
{
    playlist_delete_path(playlist, path);
}

void climpd_player_delete_media(struct media *m)
{
    playlist_delete_media(playlist, m);
}

void climpd_player_take_media(struct media *m)
{
    playlist_take_media(playlist, m);
}

void climpd_player_set_volume(unsigned int volume)
{
    volume = max(volume, 0);
    volume = min(volume, 120);
    
    media_player_set_volume(&media_player, volume);
}

unsigned int climpd_player_volume(void)
{
    return media_player_volume(&media_player);
}

void climpd_player_set_muted(bool muted)
{
    media_player_set_muted(&media_player, muted);
}

bool climpd_player_muted(void)
{
    return media_player_muted(&media_player);
}

void climpd_player_set_repeat(bool repeat)
{
    playlist_set_repeat(playlist, repeat);
}

bool climpd_player_repeat(void)
{
    return playlist_repeat(playlist);
}

void climpd_player_set_shuffle(bool shuffle)
{
    playlist_set_shuffle(playlist, shuffle);
}

bool climpd_player_shuffle(void)
{
    return playlist_shuffle(playlist);
}

bool climpd_player_playing(void)
{
    return media_player_playing(&media_player);
}

bool climpd_player_paused(void)
{
    return media_player_paused(&media_player);
}

bool climpd_player_stopped(void)
{
    return media_player_stopped(&media_player);
}

const struct media *climpd_player_current(void)
{
    return playlist_current(playlist);
}

void climpd_player_set_playlist(struct playlist *pl)
{
    playlist_set_repeat(pl, playlist_repeat(playlist));
    playlist_set_shuffle(pl, playlist_shuffle(playlist));
    
    playlist = pl;
}

void climpd_player_print_playlist(int fd)
{
    const struct link *link;
    const struct media *m, *current;
    unsigned int index;
    
    current = playlist_current(playlist);
    index   = 0;
    
    playlist_for_each(playlist, link) {
        index += 1;
        m = container_of(link, struct media, link);
        
        if(m == current)
            print_media(fd, index, m, conf.media_active_color);
        else
            print_media(fd, index, m, conf.media_passive_color);
    }
}

void climpd_player_print_files(int fd)
{
    struct link *link;
    
    playlist_for_each(playlist, link)
        dprintf(fd, "%s\n", container_of(link, struct media, link)->path);
}

void climpd_player_print_current_track(int fd)
{
    const struct media *m;
    unsigned int index;
    
    m = playlist_current(playlist);
    if(!m) {
        dprintf(fd, "No current track available\n");
        return;
    }
    
    index = playlist_index_of(playlist, m);
    if(index < 0)
        climpd_log_w("Current track %s not in playlist\n", m->path);
    
    print_media(fd, index, m, conf.media_active_color);
}

void climpd_player_print_volume(int fd)
{
    dprintf(fd, " Volume: %u\n", media_player_volume(&media_player));
}

void climpd_player_print_state(int fd)
{
    const struct media *m;
    
    m = playlist_current(playlist);
    
    if(media_player_playing(&media_player))
        dprintf(fd, " climpd-player playing\t~ %s ~\n", m->path);
    else if(media_player_paused(&media_player))
        dprintf(fd, " climpd-player is paused on\t~ %s ~\n", m->path);
    else if(media_player_stopped(&media_player))
        dprintf(fd, " climpd-player is stopped\n");
    else
        dprintf(fd, " climpd-player state is unknown\n");
}