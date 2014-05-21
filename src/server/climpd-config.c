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

#include "climpd-log.h"
#include "climpd-config.h"
#include "terminal-color-map.h"
#include "bool-map.h"

#include "../shared/util.h"

#define DEFAULT_STR_DEFAULT_PLAYLIST     "None"
#define DEFAULT_STR_MEDIA_META_LENGTH    "24"
#define DEFAULT_STR_MEDIA_ACTIVE_COLOR   "green"
#define DEFAULT_STR_MEDIA_PASSIVE_COLOR  "default"
#define DEFAULT_STR_VOLUME               "100"
#define DEFAULT_STR_REPEAT               "false"
#define DEFAULT_STR_SHUFFLE              "false"

#define DEFAULT_VAL_DEFAULT_PLAYLIST    DEFAULT_STR_DEFAULT_PLAYLIST
#define DEFAULT_VAL_MEDIA_META_LENGTH   24
#define DEFAULT_VAL_MEDIA_ACTIVE_COLOR  COLOR_CODE_GREEN
#define DEFAULT_VAL_MEDIA_PASSIVE_COLOR COLOR_CODE_DEFAULT
#define DEFAULT_VAL_VOLUME              100
#define DEFAULT_VAL_REPEAT              false
#define DEFAULT_VAL_SHUFFLE             false

struct climpd_config conf = {
    .default_playlist    = DEFAULT_VAL_DEFAULT_PLAYLIST,
    .media_active_color  = DEFAULT_VAL_MEDIA_ACTIVE_COLOR,
    .media_passive_color = DEFAULT_VAL_MEDIA_PASSIVE_COLOR,
    .media_meta_length   = DEFAULT_VAL_MEDIA_META_LENGTH,
    .volume              = DEFAULT_VAL_VOLUME,
    .repeat              = DEFAULT_VAL_REPEAT,
    .shuffle             = DEFAULT_VAL_SHUFFLE
};

static const char default_config_text[] = {
    "\n"
    "# Set automatically loaded playlist\n"
    "; DefaultPlaylist = \n"
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

static struct config *config;

static void set_default_playlist(const char *str)
{
    struct stat st;
    int err;
    
    if(strcasecmp(str, DEFAULT_STR_DEFAULT_PLAYLIST) == 0)
        return;

    err = stat(str, &st);
    if(err < 0) {
        climpd_log_e("climpd-config: %s: %s\n", str, errno_string(errno));
        return;
    }
    
    if(!S_ISREG(st.st_mode)) {
        climpd_log_w("climpd-config: %s is not a file\n", str);
        return;
    }
    conf.default_playlist = str;
}

static void set_media_meta_length(const char *str)
{
    unsigned int val;
    
    errno = 0;
    val = (unsigned int) strtol(str, NULL, 10);
    
    if(errno) {
        climpd_log_w("climpd-config: invalid MediaMetaLength %s\n", str);
        val = DEFAULT_VAL_MEDIA_META_LENGTH;
        return;
    }
        
    conf.media_meta_length = val;
}

static void set_media_active_color(const char *str)
{
    const char *val;
    
    val = terminal_color_map_color_code(str);
    if(!val) {
        climpd_log_w("climpd-config: invalid MediaActiceColor %s\n", str);
        val = DEFAULT_VAL_MEDIA_ACTIVE_COLOR;
    }
    
    conf.media_active_color = val;
}

static void set_media_passive_color(const char *str)
{
    const char *val;
    
    val = terminal_color_map_color_code(str);
    if(!val) {
        climpd_log_w("climpd-config: invalid MediaPassiveColor %s\n", str);
        val = DEFAULT_VAL_MEDIA_PASSIVE_COLOR;
    }
    
    conf.media_passive_color = val;
}

static void set_volume(const char *str)
{
    unsigned int val;
    
    errno = 0;
    val = (unsigned int) strtol(str, NULL, 10);
    
    if(errno) {
        climpd_log_w("climpd-config: invalid Volume %s\n", str);
        val = DEFAULT_VAL_VOLUME;
    }
    
    conf.volume = val;
}

static void set_repeat(const char *str)
{
    const bool *val;
    
    val = bool_map_value(str);
    if(!val) {
        climpd_log_w("climpd-config: invalid Repeat %s\n", str);
        conf.repeat = DEFAULT_VAL_REPEAT;
        return;
    }
    
    conf.repeat = *val;
}

static void set_shuffle(const char *str)
{
    const bool *val;
    
    val = bool_map_value(str);
    if(!val) {
        climpd_log_w("climpd-config: invalid Shuffle %s\n", str);
        conf.shuffle = DEFAULT_VAL_SHUFFLE;
        return;
    }
    
    conf.shuffle = *val;
    
}

int climpd_config_init(void)
{
    static const char *dir  = "/.config/climp/";
    static const char *path = "/.config/climp/climpd.conf";
    char *config_dir, *config_path;
    char *home, *p;
    size_t home_len;
    int fd, mode, err;
    
    /* Init some path variables */

    home = getenv("HOME");
    home_len = strlen(home);
    
    config_dir = malloc(home_len + strlen(dir));
    if(!config_dir)
        goto out;
    
    config_dir = strcpy(config_dir, home);
    config_dir = strcat(config_dir, dir);
    
    config_path = malloc(home_len + strlen(path));
    if(!config_path)
        goto cleanup1;
    
    config_path = strcpy(config_path, home);
    config_path = strcat(config_path, path);
    
    /* Create folders if necessary */
        
    for(p = config_dir + home_len; *p != '\0'; ++p) {
        if(*p == '/') {
            *p = '\0';
            
            err = mkdir(config_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            
            *p = '/';
            
            if(err < 0) {
                err = -errno;
                if(err != -EEXIST && err != -EPERM)
                    goto cleanup2;
            }
        }
    }
    
    /* Create and initialize file if necessary */
    
    mode = S_IRUSR | S_IWUSR | S_IRGRP;

    fd = open(config_path, O_CREAT | O_EXCL | O_WRONLY,  mode);
    
    if(fd < 0) {
        err = -errno;
        if(err != -EEXIST)
            goto cleanup2;
        
    } else {
        dprintf(fd, "%s\n", default_config_text);
        close(fd);
    }
    
    /* Read config file */
    
    config = config_new(config_path);
    if(!config) {
        err = -errno;
        goto cleanup2;
    }

    err = climpd_config_reload();
    if(err < 0)
        goto cleanup3;
    
    free(config_path);
    free(config_dir);
    
    return 0;

cleanup3:
    config_delete(config);
cleanup2:
    free(config_path);
cleanup1:
    free(config_dir);
out:
    return err;
}

int climpd_config_reload(void)
{
    const char *str;
    int err;
    
    err = config_parse(config);
    if(err < 0)
        return err;
     
    str = config_value(config, "DefaultPlaylist");
    if(str)
        set_default_playlist(str);
    
    str = config_value(config, "MediaMetaLength");
    if(str)
        set_media_meta_length(str);
    
    str = config_value(config, "MediaActiveColor");
    if(str)
        set_media_active_color(str);
    
    str = config_value(config, "MediaPassiveColor");
    if(str)
        set_media_passive_color(str);

    str = config_value(config, "Volume");
    if(str)
        set_volume(str);

    str = config_value(config, "Repeat");
    if(str)
        set_repeat(str);
    
    str = config_value(config, "Shuffle");
    if(str)
        set_shuffle(str);

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

void climpd_config_destroy(void)
{
    config_delete(config);
}
