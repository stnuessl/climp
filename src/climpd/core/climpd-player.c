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
//  * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <assert.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/vector.h>
#include <libvci/random.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/media-player/media-player.h>
#include <core/media-player/media-discoverer.h>
#include <core/terminal-color-map.h>
#include <core/climpd-player.h>
#include <core/climpd-paths.h>
#include <core/util.h>

#include <obj/playlist.h>
#include <obj/uri.h>

static const char *tag = "climpd-player";

static struct playlist _playlist;
static struct media *_running_track;

static void handle_end_of_stream(void)
{
    int err;
    
    if (_running_track)
        media_unref(_running_track);
    
    _running_track = playlist_next(&_playlist);
    if(!_running_track) {
        climpd_log_i(tag, "finished playing current playlist\n");
        return;
    }
    
    err = media_player_play(media_uri(_running_track));
    if(err < 0)
        climpd_log_e(tag, "encountered an error - stopping playback\n");
}

// static void print_media(struct media *__restrict m, unsigned int index, int fd) 
// {
//     GstDiscovererResult result;
//     const struct media_info *i;
//     unsigned int min, sec, meta_len;
//     const char *color, *title, *artist, *album;
//     char *err_msg;
//     
//     result = media_parse(m, _gst_discoverer, &err_msg);
//     if (result != GST_DISCOVERER_OK) {
//         const char *uri = media_uri(m);
//         
//         climpd_log_e(tag, "failed to parse \"%s\" - %s\n", uri, err_msg);
//         dprintf(fd, "climpd: failed to parse %s - %s\n", uri, err_msg);
//         
//         free(err_msg);
//         return;
//     }
// 
//     i = media_info(m);
//     
//     title    = i->title;
//     artist   = i->artist;
//     album    = i->album;
// 
//     if (m == _running_track)
//         color = climpd_config_media_active_color();
//     else
//         color = climpd_config_media_passive_color();
//     
//     min = i->duration / 60;
//     sec = i->duration % 60;
//     
//     meta_len = climpd_config_media_meta_length();
//     
//     if (isatty(fd)) {
//         dprintf(fd, "%s ( %3u )    %2u:%02u   %-*.*s %-*.*s %-*.*s\n"
//                 COLOR_CODE_DEFAULT, color, index, min, sec,
//                 meta_len, meta_len, title, 
//                 meta_len, meta_len, artist, 
//                 meta_len, meta_len, album);
//     } else {
//         dprintf(fd, " ( %3u )    %2u:%02u   %-*.*s %-*.*s %-*.*s\n", 
//                 index, min, sec,
//                 meta_len, meta_len, title, 
//                 meta_len, meta_len, artist, 
//                 meta_len, meta_len, album);
//     }
// }

static void load_playlist_from_disk(void)
{
    const char *path = climpd_paths_last_playlist();
    struct media_list ml;
    int err;

    err = media_list_init(&ml);
    if (err < 0) {
        climpd_log_w(tag, "failed to initialize last played playlist\n");
        return;
    }
    
    err = media_list_add_from_path(&ml, path);
    if (err < 0 && err != -ENOENT) {
        climpd_log_w(tag, "failed to load last playlist elements from \"%s\" - "
                     "%s\n", path, strerr(-err));
        goto cleanup;
    }
    
    err = playlist_add_media_list(&_playlist, &ml);
    if (err < 0) {
        climpd_log_w(tag, "failed to add last playlist elements - %s\n", 
                     strerr(-err));
        goto cleanup;
    }
    
    media_discoverer_discover_media_list_async(&ml);
    
cleanup:
    media_list_destroy(&ml);
}

static void save_playlist_to_disk(void)
{
    const char *path = climpd_paths_last_playlist();
    int fd;
    
    fd = open(path, O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        climpd_log_e(tag, "failed to save playlist to \"%s\" - %s\n", path, 
                     errstr);
        return;
    } 
    
    climpd_player_print_uris(fd);
    close(fd);
}

void climpd_player_init(void)
{
    bool repeat, shuffle;
    int err;
    
    media_player_init(&handle_end_of_stream);
    media_discoverer_init();
    
    repeat  = climpd_config_repeat();
    shuffle = climpd_config_shuffle();
    
    playlist_init(&_playlist, repeat, shuffle);
    
    load_playlist_from_disk();

    climpd_log_i(tag, "initialized");
    
    return;
fail:
    die_failed_init(tag);
}

void climpd_player_destroy(void)
{
    media_player_stop();
    
    playlist_destroy(&_playlist);
    media_discoverer_destroy();
    media_player_destroy();
    climpd_log_i(tag, "destroyed\n");
}

int climpd_player_play_path(const char *path)
{
    struct media *m;
    unsigned int index;
    int err;
    
    index = playlist_index_of_path(&_playlist, path);
    if (index != (unsigned int) -1)
        return climpd_player_play_track((int) index);

    m = media_new(path);
    if (!m)
        return -errno;
    
    err = playlist_insert_back(&_playlist, m);
    if (err < 0)
        goto cleanup1;
    
    if (_running_track)
        media_unref(_running_track);
    
    _running_track = m;
    
    /* 
     * We don't need to update the index since it is at the last position
     * and after that song the playback would finish.
     */
    
    err = media_player_play(media_uri(m));
    if (err < 0)
        goto cleanup1;
    
    return 0;
    
cleanup1:
    media_unref(m);
    return err;
}

int climpd_player_play_track(int index)
{
    if ((unsigned int) abs(index) >= playlist_size(&_playlist))
        return -EINVAL;
    
    if (_running_track)
        media_unref(_running_track);
    
    _running_track = playlist_at(&_playlist, index);
    playlist_update_index(&_playlist, index);
    
    return media_player_play(media_uri(_running_track));
}

int climpd_player_play(void)
{
    const char *uri;
    int err;
    
    switch (_gst_state) {
    case GST_STATE_PLAYING:
        break;
    case GST_STATE_PAUSED:
        uri = media_uri(_running_track);
        
        err = climpd_player_set_state(GST_STATE_PLAYING);
        if(err < 0) {
            climpd_log_e(tag, "failed to resume '%s'.\n", uri);
            return err;
        }
        
        climpd_log_i(tag, "resuming '%s'.\n", uri); 
        break;
    case GST_STATE_NULL:
    default:
        if (_running_track)
            media_unref(_running_track);
        
        _running_track = playlist_next(&_playlist);
        if (!_running_track)
            return -ENOENT;

        err = climpd_player_play_uri(media_uri(_running_track));
        if (err < 0)
            return err;

        break;
    }
    
    return 0;
}

void climpd_player_pause(void)
{
    media_player_pause();
}

void climpd_player_stop(void)
{
    media_player_stop();
}

int climpd_player_next(void)
{
    if (_running_track)
        media_unref(_running_track);
    
    _running_track = playlist_next(&_playlist);

    if (!_running_track) {
        climpd_player_stop();
        return 0;
    }
    
    return media_player_play(media_uri(_running_track));
}

int climpd_player_previous(void)
{
    assert(0 && "NOT IMPLEMENTED");
    
    return 0;
}

int climpd_player_peek(void)
{
    return media_player_peek();
}

int climpd_player_seek(unsigned int sec)
{
    const struct media_info *i;
    
    if (!_running_track)
        return -ENOENT;
    
    i = media_info(_running_track);
    
    if (media_is_parsed(_running_track) && !i->seekable)
        return -ENOTSUP;
    
    if (sec >= i->duration)
        return -EINVAL;
    
    return media_player_seek(sec);
}

int climpd_player_insert(const char *__restrict path)
{
    return playlist_emplace_back(&_playlist, path);
}

void climpd_player_delete_index(int index)
{
    if ((unsigned int) abs(index) < playlist_size(&_playlist))
        media_unref(playlist_take(&_playlist, index));
}

void climpd_player_delete_media(const struct media *__restrict m)
{
    unsigned int index = playlist_index_of(&_playlist, m);
    if (index == (unsigned int) -1)
        return;
    
    media_unref(playlist_take(&_playlist, (int) index));
}

void climpd_player_set_volume(unsigned int vol)
{
    media_player_set_volume(vol);
}

unsigned int climpd_player_volume(void)
{
    return media_player_volume();
}

void climpd_player_set_muted(bool mute)
{
    media_player_set_muted(mute);
}

bool climpd_player_muted(void)
{
    return media_player_muted();
}

bool climpd_player_toggle_muted(void)
{
    bool mute = !climpd_player_muted();
    
    climpd_player_set_muted(mute);
    
    return mute;
}

void climpd_player_set_repeat(bool repeat)
{
    playlist_set_repeat(&_playlist, repeat);
}

bool climpd_player_repeat(void)
{
    return playlist_repeat(&_playlist);
}

bool climpd_player_toggle_repeat(void)
{
    return playlist_toggle_repeat(&_playlist);
}

void climpd_player_set_shuffle(bool shuffle)
{
    playlist_set_shuffle(&_playlist, shuffle);
}

bool climpd_player_shuffle(void)
{
    return playlist_shuffle(&_playlist);
}

bool climpd_player_toggle_shuffle(void)
{
    return playlist_toggle_shuffle(&_playlist);
}

enum climpd_player_state climpd_player_state(void)
{
    return _gst_state;
}

bool climpd_player_is_playing(void)
{
    return _gst_state == GST_STATE_PLAYING;
}

bool climpd_player_is_paused(void)
{
    return _gst_state == GST_STATE_PAUSED;
}

bool climpd_player_is_stopped(void)
{
    return _gst_state == GST_STATE_NULL;
}

int climpd_player_add_media_list(struct media_list *__restrict ml)
{
    int err = playlist_add_media_list(&_playlist, ml);
    if (err < 0) {
        climpd_log_e(tag, "adding media list failed - %s\n", strerr(-err));
        return err;
    }
    
    media_discoverer_discover_media_list_async(ml);
        
    return err;
}

int climpd_player_set_media_list(struct media_list *__restrict ml)
{
    int err = playlist_set_media_list(&_playlist, ml);
    if (err < 0) {
        climpd_log_e(tag, "setting media list failed - %s\n", strerr(-err));
        return err;
    }
    
    media_discoverer_discover_media_async(ml);
    
    return 0;
}

void climpd_player_remove_media_list(struct media_list *__restrict ml)
{
    playlist_remove_media_list(&_playlist, ml);
}

void climpd_player_clear_playlist(void)
{
    playlist_clear(&_playlist);
}

void climpd_player_sort_playlist(void)
{
    playlist_sort(&_playlist);
}

// void climpd_player_print_current_track(int fd)
// {
//     if (_running_track)
//         print_media(_running_track, playlist_index(&_playlist), fd);
// }
// 
// void climpd_player_print_playlist(int fd)
// {
//     unsigned int size = playlist_size(&_playlist);
//     
//     for (unsigned int i = 0; i < size; ++i) {
//         struct media *m = playlist_at(&_playlist, i);
//         
//         print_media(m, i, fd);
//         
//         media_unref(m);
//     }
//     
//     dprintf(fd, "\n");
// }
// 
// void climpd_player_print_files(int fd)
// {
//     unsigned int size = playlist_size(&_playlist);
//     
//     for (unsigned int i = 0; i < size; ++i) {
//         struct media *m = playlist_at(&_playlist, i);
//         
//         dprintf(fd, "%s\n", media_hierarchical(m));
//         
//         media_unref(m);
//     }
//     
//     dprintf(fd, "\n");
// }
// 
// void climpd_player_print_uris(int fd)
// {
//     unsigned int size = playlist_size(&_playlist);
//     
//     for (unsigned int i = 0; i < size; ++i) {
//         struct media *m = playlist_at(&_playlist, i);
//         
//         dprintf(fd, "%s\n", media_uri(m));
//         
//         media_unref(m);
//     }
//     
//     dprintf(fd, "\n");
// }
// 
// void climpd_player_print_volume(int fd)
// {
//     dprintf(fd, " Volume: %u\n", _volume);
// }
