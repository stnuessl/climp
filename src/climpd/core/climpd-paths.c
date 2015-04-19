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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/util.h>

static const char *tag = "climpd-paths";

static char *_config_path;
static char *_media_loader_path;
static char *_last_playlist_path;

#define CONFIG_PATH ".config/climp/climpd.conf"
#define MEDIA_LOADER_PATH ".config/climp/playlists/"
#define LAST_PLAYLIST_PATH ".config/climp/playlists/__last_playlist.m3u"


static void set_path(char *__restrict dst, 
                     const char *__restrict home,
                     const char *__restrict path)
{
    strcpy(dst, home);
    strcat(dst, "/");
    strcat(dst, path);
}

void climpd_paths_init(void)
{
    const char *home;
    size_t len;
    
    home = getenv("HOME");
    if (!home) {
        climpd_log_e(tag, "no home directory for user %d\n", getuid());
        goto fail;
    }
    
    len = strlen(home);
    
    _config_path = malloc(sizeof(CONFIG_PATH) + len);
    if (!_config_path) {
        climpd_log_e(tag, "failed to set up config path - %s\n", errstr);
        goto fail;
    }
    
    set_path(_config_path, home, CONFIG_PATH);
    
    _media_loader_path = malloc(sizeof(MEDIA_LOADER_PATH) + len);
    if (!_media_loader_path) {
        climpd_log_e(tag, "failed to set up media loader path - %s\n", errstr);
        goto fail;
    }

    set_path(_media_loader_path, home, MEDIA_LOADER_PATH);
    
    _last_playlist_path = malloc(sizeof(LAST_PLAYLIST_PATH) + len);
    if (!_last_playlist_path) {
        climpd_log_e(tag, "failed to set up last playlist path - %s\n", errstr);
        goto fail;
    }

    set_path(_last_playlist_path, home, LAST_PLAYLIST_PATH);
    
    climpd_log_i(tag, "initialized\n");
    return;

fail:
    die_failed_init(tag);
}

void climpd_paths_destroy(void)
{
    free(_last_playlist_path);
    free(_media_loader_path);
    free(_config_path);
    
    climpd_log_i(tag, "destroyed\n");
}

const char *climpd_paths_config(void)
{
    return _config_path;
}

const char *climpd_paths_media_list_loader(void)
{
    return _media_loader_path;
}

const char *climpd_paths_last_playlist(void)
{
    return _last_playlist_path;
}
