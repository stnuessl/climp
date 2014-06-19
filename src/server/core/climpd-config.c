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
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libvci/config.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include "util/climpd-log.h"
#include "util/terminal-color-map.h"
#include "util/bool-map.h"

#include "core/climpd-config.h"

#include "../../shared/util.h"
#include "../../shared/constants.h"

#define DEFAULT_STR_PLAYLIST_FILE        ""
#define DEFAULT_STR_SAVE_PLAYLIST_FILE   "false"
#define DEFAULT_STR_DEFAULT_PLAYLIST     ""
#define DEFAULT_STR_MEDIA_META_LENGTH    "24"
#define DEFAULT_STR_MEDIA_ACTIVE_COLOR   "green"
#define DEFAULT_STR_MEDIA_PASSIVE_COLOR  "default"
#define DEFAULT_STR_VOLUME               "100"
#define DEFAULT_STR_REPEAT               "false"
#define DEFAULT_STR_SHUFFLE              "false"

#define DEFAULT_VAL_PLAYLIST_FILE       NULL
#define DEFAULT_VAL_SAVE_PLAYLIST_FILE  false
#define DEFAULT_VAL_DEFAULT_PLAYLIST    NULL
#define DEFAULT_VAL_MEDIA_META_LENGTH   24
#define DEFAULT_VAL_MEDIA_ACTIVE_COLOR  COLOR_CODE_GREEN
#define DEFAULT_VAL_MEDIA_PASSIVE_COLOR COLOR_CODE_DEFAULT
#define DEFAULT_VAL_VOLUME              100
#define DEFAULT_VAL_REPEAT              false
#define DEFAULT_VAL_SHUFFLE             false

struct climpd_config conf = {
    .playlist_file       = DEFAULT_VAL_PLAYLIST_FILE,
    .save_playlist_file  = DEFAULT_VAL_SAVE_PLAYLIST_FILE,
    .default_playlist    = DEFAULT_VAL_DEFAULT_PLAYLIST,
    .media_active_color  = DEFAULT_VAL_MEDIA_ACTIVE_COLOR,
    .media_passive_color = DEFAULT_VAL_MEDIA_PASSIVE_COLOR,
    .media_meta_length   = DEFAULT_VAL_MEDIA_META_LENGTH,
    .volume              = DEFAULT_VAL_VOLUME,
    .repeat              = DEFAULT_VAL_REPEAT,
    .shuffle             = DEFAULT_VAL_SHUFFLE
};

static const char *tag = "climpd-config";
static struct config *config;

static void set_playlist_file(const char *key, const char *val, void *arg)
{
    struct stat st;
    int err;
    
    (void) key;
    (void) arg;
    
    err = stat(val, &st);
    if(err < 0) {
        climpd_log_w(tag, "%s: %s\n", val, strerr(errno));
        return;
    }
    
    conf.playlist_file = val;
}

static void set_save_playlist_file(const char *key, const char *val, void *arg)
{
    const bool *bval;
    
    (void) key;
    (void) arg;
    
    bval = bool_map_value(val);
    if(!bval) {
        climpd_log_w(tag, "unkown boolean value '%s'\n", val);
        return;
    }
    
    conf.save_playlist_file = *bval;
}

static void set_default_playlist(const char *key, const char *val, void *arg)
{
    struct stat st;
    int err;
    
    (void) key;
    (void) arg;
    
    if(strcasecmp(val, DEFAULT_STR_DEFAULT_PLAYLIST) == 0)
        return;
    
    err = stat(val, &st);
    if(err < 0) {
        climpd_log_e(tag, "%s: %s\n", val, strerr(errno));
        return;
    }
    
    if(!S_ISREG(st.st_mode)) {
        climpd_log_w(tag, "%s is not a file\n", val);
        return;
    }
    
    conf.default_playlist = (val[0] != '/') ? val : strrchr(val, '/') + 1;
}

static void set_media_meta_length(const char *key, const char *val, void *arg)
{
    unsigned int num;
    
    (void) key;
    (void) arg;
    
    errno = 0;
    num = (unsigned int) strtol(val, NULL, 10);
    
    if(errno) {
        climpd_log_w(tag, "invalid MediaMetaLength %s\n", val);
        num = DEFAULT_VAL_MEDIA_META_LENGTH;
        return;
    }
    
    conf.media_meta_length = num;
}

static void set_media_active_color(const char *key, const char *val, void *arg)
{
    const char *code;
    
    (void) key;
    (void) arg;
    
    code = terminal_color_map_color_code(val);
    if(!code) {
        climpd_log_w(tag, "invalid MediaActiceColor %s\n", val);
        code = DEFAULT_VAL_MEDIA_ACTIVE_COLOR;
    }
    
    conf.media_active_color = code;
}

static void set_media_passive_color(const char *key, const char *val, void *arg)
{
    const char *code;
    
    (void) key;
    (void) arg;
    
    code = terminal_color_map_color_code(val);
    if(!code) {
        climpd_log_w(tag, "invalid MediaPassiveColor %s\n", val);
        code = DEFAULT_VAL_MEDIA_PASSIVE_COLOR;
    }
    
    conf.media_passive_color = code;
}

static void set_volume(const char *key, const char *val, void *arg)
{
    unsigned int num;
    
    (void) key;
    (void) arg;
    
    errno = 0;
    num = (unsigned int) strtol(val, NULL, 10);
    
    if(errno) {
        climpd_log_w(tag, "invalid Volume %s\n", val);
        num = DEFAULT_VAL_VOLUME;
    }
    
    conf.volume = num;
}

static void set_repeat(const char *key, const char *val, void *arg)
{
    const bool *bval;
    
    (void) key;
    (void) arg;
    
    bval = bool_map_value(val);
    if(!bval) {
        climpd_log_w(tag, "invalid Repeat %s\n", val);
        conf.repeat = DEFAULT_VAL_REPEAT;
        return;
    }
    
    conf.repeat = *bval;
}

static void set_shuffle(const char *key, const char *val, void *arg)
{
    const bool *bval;
    
    (void) key;
    (void) arg;
    
    bval = bool_map_value(val);
    if(!bval) {
        climpd_log_w(tag, "invalid Shuffle %s\n", val);
        conf.shuffle = DEFAULT_VAL_SHUFFLE;
        return;
    }
    
    conf.shuffle = *bval;
}

static struct config_handle handles[] = {
    { &set_playlist_file,          "PlaylistFile",         NULL },
    { &set_save_playlist_file,     "SavePlaylistFile",     NULL },
    { &set_default_playlist,       "DefaultPlaylist",      NULL },
    { &set_media_active_color,     "MediaActiveColor",     NULL },
    { &set_media_passive_color,    "MediaPassiveColor",    NULL },
    { &set_media_meta_length,      "MediaMetaLength",      NULL },
    { &set_volume,                 "Volume",               NULL },
    { &set_repeat,                 "Repeat",               NULL },
    { &set_shuffle,                "Shuffle",              NULL }
};

static const char default_config_text[] = {
    "\n"
    "# Set automatically loaded playlists\n"
    ";PlaylistFile = "DEFAULT_STR_PLAYLIST_FILE"\n"
    "\n"
    "# Save changes in playlist file?\n"
    ";SavePlaylistFile = "DEFAULT_STR_SAVE_PLAYLIST_FILE"\n"
    "\n"
    "# Set default played playlist\n"
    ";DefaultPlaylist = "DEFAULT_STR_DEFAULT_PLAYLIST"\n"
    "\n"
    "# Table width of the playlist\n"
    "MediaMetaLength = "DEFAULT_STR_MEDIA_META_LENGTH"\n"
    "\n"
    "# When printed, color of the currently played track\n"
    "MediaActiveColor = "DEFAULT_STR_MEDIA_ACTIVE_COLOR"\n"
    "\n"
    "# When printed, color of the tracks\n"
    "MediaPassiveColor = "DEFAULT_STR_MEDIA_PASSIVE_COLOR"\n"
    "\n"
    "# Set the volume of the audio output\n"
    "Volume = "DEFAULT_STR_VOLUME"\n"
    "\n"
    "# Repeat the playlist?\n"
    "Repeat = "DEFAULT_STR_REPEAT"\n"
    "\n"
    "# Play songs in random order?\n"
    "Shuffle = "DEFAULT_STR_SHUFFLE"\n"
};


int climpd_config_init(void)
{
    static const char *dir  = "/.config/climp/";
    static const char *path = "/.config/climp/climpd.conf";
    char *config_dir, *config_path;
    char *home;
    size_t home_len;
    int i, fd, err;
    
    /* Init some path variables */
    home = getenv("HOME");
    if(!home) {
        err = -ENOENT;
        climpd_log_e(tag, "no home directory for user %d\n", getuid());
        goto out;
    }
    
    home_len = strlen(home);
    
    config_dir = malloc(home_len + strlen(dir) + 1);
    if(!config_dir) {
        climpd_log_e(tag, "malloc(): %s\n", errstr);
        goto out;
    }
    
    config_dir = strcpy(config_dir, home);
    config_dir = strcat(config_dir, dir);
    
    config_path = malloc(home_len + strlen(path) + 1);
    if(!config_path) {
        climpd_log_e(tag, "malloc(): %s\n", errstr);
        goto cleanup1;
    }
    
    config_path = strcpy(config_path, home);
    config_path = strcat(config_path, path);
    err = create_leading_directories(config_dir, DEFAULT_DIR_MODE);
    if(err < 0) {
        climpd_log_e(tag, "create_leading_directories(): %s\n", strerr(-err));
        goto cleanup2;
    }
    
    /* Create and initialize file if necessary */

    fd = open(config_path, O_CREAT | O_EXCL | O_WRONLY,  DEFAULT_FILE_MODE);
    if(fd < 0) {
        if(errno != EEXIST) {
            climpd_log_e(tag, "open(): %s\n", errstr);
            err = -errno;
            goto cleanup2;
        }
    } else {
        dprintf(fd, "%s\n", default_config_text);
        close(fd);
    }
    
    /* Read config file */
    
    config = config_new(config_path);
    if(!config) {
        err = -errno;
        climpd_log_e(tag, "config_new(): %s\n", strerr(errno));
        goto cleanup2;
    }
    
    for(i = 0; i < ARRAY_SIZE(handles); ++i) {
        err = config_insert_handle(config, handles + i);
        if(err < 0)
            goto cleanup3;
    }

    err = climpd_config_reload();
    if(err < 0) {
        climpd_log_e(tag, "loading config failed: %s\n", strerr(-err));
        goto cleanup3;
    }
    
    free(config_path);
    free(config_dir);
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;

cleanup3:
    config_delete(config);
cleanup2:
    free(config_path);
cleanup1:
    free(config_dir);
out:
    climpd_log_e(tag, "initialization failed\n");
    return err;
}


void climpd_config_destroy(void)
{
    config_delete(config);
    climpd_log_i(tag, "destroyed\n");
}


int climpd_config_reload(void)
{
    int err;
    
    err = config_parse(config);
    if(err < 0)
        return err;
    
    return 0;
}

void climpd_config_print(int fd)
{
    struct entry *e;
    const char *key, *val;
    
    dprintf(fd, "\n climpd-config: %s\n", config_path(config));
    
    config_for_each(config, e) {
        key = entry_key(e);
        val = entry_data(e);
        
        dprintf(fd, "  %-20s = %s\n", key, val);
    }
    
    dprintf(fd, "\n");
}
