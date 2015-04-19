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

#include <core/climpd-log.h>
#include <core/terminal-color-map.h>
#include <core/climpd-paths.h>
#include <core/climpd-config.h>
#include <core/util.h>

#define DEFAULT_VAL_MEDIA_META_LENGTH   24
#define DEFAULT_VAL_MEDIA_ACTIVE_COLOR  COLOR_CODE_GREEN
#define DEFAULT_VAL_MEDIA_PASSIVE_COLOR COLOR_CODE_DEFAULT
#define DEFAULT_VAL_VOLUME              60
#define DEFAULT_VAL_REPEAT              false
#define DEFAULT_VAL_SHUFFLE             false

static const char *tag = "climpd-config";

static struct config _config;
static unsigned int _media_meta_length;
static const char *_media_active_color;
static const char *_media_passive_color;
static unsigned int _volume;
static bool _repeat;
static bool _shuffle;

static void config_text_init(int fd, void *arg)
{
    (void) arg;
    
    dprintf(fd, "# When printed, column width of meta information\n"
                "MediaMetaLength = %u\n\n"
                "# When printed, color of the currently played track\n"
                "MediaActiveColor = %s\n\n"
                "# When printed, color of the tracks in the playlist\n"
                "MediaPassiveColor = %s\n\n"
                "# Volume of the audio output\n"
                "Volume = %u\n\n"
                "# Repeat playlist\n"
                "Repeat = %s\n\n"
                "# Play tracks in the playlist in random order\n"
                "Shuffle = %s\n\n",
            _media_meta_length, _media_active_color, _media_passive_color, 
            _volume,  true_false(_repeat), true_false(_shuffle));
}


// static void set_default_playlist(const char *key, const char *val, void *arg)
// {
//     struct climpd_config *values;
//     struct stat st;
//     int err;
//     
//     (void) key;
//     
//     values = arg;
//     
//     if(strcasecmp(val, DEFAULT_STR_DEFAULT_PLAYLIST) == 0)
//         return;
//     
//     err = stat(val, &st);
//     if(err < 0) {
//         climpd_log_e(tag, "%s: %s\n", val, strerr(errno));
//         return;
//     }
//     
//     if(!S_ISREG(st.st_mode)) {
//         climpd_log_w(tag, "%s is not a file\n", val);
//         return;
//     }
//     
//     values->default_playlist = val;
// }

static void set_media_meta_length(const char *key, const char *val, void *arg)
{
    unsigned int num;
    
    (void) key;
    (void) arg;
    
    errno = 0;
    num = (unsigned int) strtol(val, NULL, 10);
    
    if(errno) {
        climpd_log_w(tag, "invalid MediaMetaLength \"%s\"\n", val);
        num = DEFAULT_VAL_MEDIA_META_LENGTH;
        return;
    }
    
    _media_meta_length = num;
}

static void set_media_active_color(const char *key, const char *val, void *arg)
{
    const char *code;
    
    (void) key;
    (void) arg;
    
    code = terminal_color_map_color_code(val);
    if(!code) {
        climpd_log_w(tag, "invalid MediaActiceColor \"%s\"\n", val);
        code = DEFAULT_VAL_MEDIA_ACTIVE_COLOR;
    }
    
    _media_active_color = code;
}

static void set_media_passive_color(const char *key, const char *val, void *arg)
{
    const char *code;
    
    (void) key;
    (void) arg;
    
    code = terminal_color_map_color_code(val);
    if(!code) {
        climpd_log_w(tag, "invalid MediaPassiveColor \"%s\"\n", val);
        code = DEFAULT_VAL_MEDIA_PASSIVE_COLOR;
    }
    
    _media_passive_color = code;
}

static void set_volume(const char *key, const char *val, void *arg)
{
    unsigned int num;
    
    (void) key;
    (void) arg;

    errno = 0;
    num = (unsigned int) strtol(val, NULL, 10);
    
    if(errno) {
        climpd_log_w(tag, "invalid Volume \"%s\"\n", val);
        num = DEFAULT_VAL_VOLUME;
    }
    
    _volume = num;
}

static void set_repeat(const char *key, const char *val, void *arg)
{
    (void) key;
    (void) arg;
    
    if(str_to_bool(val, &_repeat) < 0) {
        climpd_log_w(tag, "invalid Repeat \"%s\"\n", val);
        _repeat = DEFAULT_VAL_REPEAT;
        return;
    }
}

static void set_shuffle(const char *key, const char *val, void *arg)
{
    (void) key;
    (void) arg;
    
    if(str_to_bool(val, &_shuffle) < 0) {
        climpd_log_w(tag, "invalid Shuffle \"%s\"\n", val);
        _shuffle = DEFAULT_VAL_SHUFFLE;
        return;
    }
}

static struct config_handle handles[] = {
    { &set_media_active_color,     "MediaActiveColor",     NULL },
    { &set_media_passive_color,    "MediaPassiveColor",    NULL },
    { &set_media_meta_length,      "MediaMetaLength",      NULL },
    { &set_volume,                 "Volume",               NULL },
    { &set_repeat,                 "Repeat",               NULL },
    { &set_shuffle,                "Shuffle",              NULL }
};

void climpd_config_init(void)
{
    const char *path;
    int err;

    path = climpd_paths_config();
    
    _media_meta_length   = DEFAULT_VAL_MEDIA_META_LENGTH;
    _media_active_color  = DEFAULT_VAL_MEDIA_ACTIVE_COLOR;
    _media_passive_color = DEFAULT_VAL_MEDIA_PASSIVE_COLOR;
    _volume              = DEFAULT_VAL_VOLUME;
    _repeat              = DEFAULT_VAL_REPEAT;
    _shuffle             = DEFAULT_VAL_SHUFFLE;
    
    
    err = config_init(&_config, path, &config_text_init, NULL);
    if(err < 0) {
        climpd_log_e(tag, "initializing config failed - %s\n", strerr(-err));
        goto fail;
    }
    
    for(unsigned int i = 0; i < ARRAY_SIZE(handles); ++i) {
        err = config_insert_handle(&_config, handles + i);
        if(err < 0) {
            climpd_log_e(tag, "adding config handle \"%s\" failed - %s\n",
                         handles[i].key, strerr(-err));
            goto fail;
        }
    }
    
    err = climpd_config_reload();
    if(err < 0) {
        climpd_log_e(tag, "loading config failed: %s\n", strerr(-err));
        goto fail;
    }
    
    climpd_log_i(tag, "initialized\n");
    
    return;

fail:
    die_failed_init(tag);
}

void climpd_config_destroy(void)
{
    config_destroy(&_config);
    
    climpd_log_i(tag, "destroyed\n");
}

int climpd_config_reload(void)
{
    int err;
        
    err = config_parse(&_config);
    if(err < 0)
        climpd_log_w(tag, "reloading config failed - %s\n", strerr(-err));
        
    return err;
}

void climpd_config_print(int fd)
{
    struct entry *e;
    const char *key, *val;
        
    dprintf(fd, "\n climpd-config: %s\n", config_path(&_config));
    
    config_for_each(&_config, e) {
        key = entry_key(e);
        val = entry_data(e);
        
        dprintf(fd, "  %-20s = %s\n", key, val);
    }
    
    dprintf(fd, "\n");
}

unsigned int climpd_config_media_meta_length(void)
{
    return _media_meta_length;
}

const char *climpd_config_media_active_color(void)
{
    return _media_active_color;
}

const char *climpd_config_media_passive_color(void)
{
    return _media_passive_color;
}

unsigned int climpd_config_volume(void)
{
    return _volume;
}

bool climpd_config_repeat(void)
{
    return _repeat;
}

bool climpd_config_shuffle(void)
{
    return _shuffle;
}
