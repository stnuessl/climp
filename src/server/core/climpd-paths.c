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

static const char *tag = "climpd_paths";

static char *_config_path;
static char *media_loader_path;

#define CONFIG_PATH ".config/climp/climpd.conf"
#define MEDIA_LOADER_PATH ".config/climp/playlists/"

void climpd_paths_init(void)
{
    const char *home;
    size_t len;
    
    home = getenv("HOME");
    if (!home) {
        climpd_log_e(tag, "no home directory for user %d\n", getuid());
        goto out;
    }
    
    len = strlen(home);
    
    _config_path = malloc(sizeof(CONFIG_PATH) + len);
    if (!_config_path) {
        climpd_log_e(tag, "failed to set up config path - %s\n", errstr);
        goto out;
    }
    
    strcpy(_config_path, home);
    strcat(_config_path, "/");
    strcat(_config_path, CONFIG_PATH);
    
    media_loader_path = malloc(sizeof(MEDIA_LOADER_PATH) + len);
    if (!media_loader_path) {
        climpd_log_e(tag, "failed to set up media list loader path - %s\n", errstr);
        goto cleanup1;
    }

    strcpy(media_loader_path, home);
    strcat(media_loader_path, "/");
    strcat(media_loader_path, MEDIA_LOADER_PATH);
    
    climpd_log_i(tag, "initialized\n");
    return;

cleanup1:
    free(_config_path);
out:
    climpd_log_e(tag, "failed to initialize - aborting...\n");
    exit(EXIT_FAILURE);
}

void climpd_paths_destroy(void)
{
    free(media_loader_path);
    free(_config_path);
    
    climpd_log_i(tag, "destroyed\n");
}

const char *climpd_paths_config(void)
{
    return _config_path;
}

const char *climpd_paths_media_list_loader(void)
{
    return media_loader_path;
}