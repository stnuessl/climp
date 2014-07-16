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


struct climpd_config_p {
    struct config config;
    struct climpd_config values;
};

static const char *tag = "climpd-config";

static void set_default_playlist(const char *key, const char *val, void *arg)
{
    struct climpd_config *values;
    struct stat st;
    int err;
    
    (void) key;
    
    values = arg;
    
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
    
    values->default_playlist = val;
}

static void set_media_meta_length(const char *key, const char *val, void *arg)
{
    struct climpd_config *values;
    unsigned int num;
    
    (void) key;
    
    values = arg;
    
    errno = 0;
    num = (unsigned int) strtol(val, NULL, 10);
    
    if(errno) {
        climpd_log_w(tag, "invalid MediaMetaLength %s\n", val);
        num = DEFAULT_VAL_MEDIA_META_LENGTH;
        return;
    }
    
    values->media_meta_length = num;
}

static void set_media_active_color(const char *key, const char *val, void *arg)
{
    struct climpd_config *values;
    const char *code;
    
    (void) key;
    
    values = arg;
    
    code = terminal_color_map_color_code(val);
    if(!code) {
        climpd_log_w(tag, "invalid MediaActiceColor %s\n", val);
        code = DEFAULT_VAL_MEDIA_ACTIVE_COLOR;
    }
    
    values->media_active_color = code;
}

static void set_media_passive_color(const char *key, const char *val, void *arg)
{
    struct climpd_config *values;
    const char *code;
    
    (void) key;
    
    values = arg;
    
    code = terminal_color_map_color_code(val);
    if(!code) {
        climpd_log_w(tag, "invalid MediaPassiveColor %s\n", val);
        code = DEFAULT_VAL_MEDIA_PASSIVE_COLOR;
    }
    
    values->media_passive_color = code;
}

static void set_volume(const char *key, const char *val, void *arg)
{
    struct climpd_config *values;
    unsigned int num;
    
    (void) key;
    
    values = arg;
    
    errno = 0;
    num = (unsigned int) strtol(val, NULL, 10);
    
    if(errno) {
        climpd_log_w(tag, "invalid Volume %s\n", val);
        num = DEFAULT_VAL_VOLUME;
    }
    
    values->volume = num;
}

static void set_repeat(const char *key, const char *val, void *arg)
{
    struct climpd_config *values;
    const bool *bval;
    
    (void) key;
    
    values = arg;
    
    bval = bool_map_value(val);
    if(!bval) {
        climpd_log_w(tag, "invalid Repeat %s\n", val);
        values->repeat = DEFAULT_VAL_REPEAT;
        return;
    }
    
    values->repeat = *bval;
}

static void set_shuffle(const char *key, const char *val, void *arg)
{
    struct climpd_config *values;
    const bool *bval;
    
    (void) key;

    values = arg;
    
    bval = bool_map_value(val);
    if(!bval) {
        climpd_log_w(tag, "invalid Shuffle %s\n", val);
        values->shuffle = DEFAULT_VAL_SHUFFLE;
        return;
    }
    
    values->shuffle = *bval;
}

static struct config_handle handles[] = {
    { &set_default_playlist,       "DefaultPlaylist",      NULL },
    { &set_media_active_color,     "MediaActiveColor",     NULL },
    { &set_media_passive_color,    "MediaPassiveColor",    NULL },
    { &set_media_meta_length,      "MediaMetaLength",      NULL },
    { &set_volume,                 "Volume",               NULL },
    { &set_repeat,                 "Repeat",               NULL },
    { &set_shuffle,                "Shuffle",              NULL }
};

static const char default_config_text[] = {
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

struct climpd_config *climpd_config_new(const char *__restrict name)
{
    struct climpd_config_p *cc;
    char *config_path, *home;
    size_t home_len;
    int i, fd, err;
    
    home = getenv("HOME");
    if(!home) {
        err = -ENOENT;
        climpd_log_e(tag, "no home directory for user %d\n", getuid());
        goto out;
    }
    
    cc = malloc(sizeof(*cc));
    if(!cc) {
        err = -errno;
        climpd_log_e(tag, "malloc() - %s\n", errstr);
        goto out;
    }

    /* Init some path variables */
    home_len = strlen(home);
    
    config_path = malloc(home_len + strlen(name) + 2);
    if(!config_path) {
        err = -errno;
        climpd_log_e(tag, "malloc() - %s\n", errstr);
        goto cleanup1;
    }
    
    config_path = strcpy(config_path, home);
    config_path = strcat(config_path, "/");
    config_path = strcat(config_path, name);
    
    err = create_leading_directories(config_path, DEFAULT_DIR_MODE);
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
    err = config_init(&cc->config, config_path);
    if(err < 0) {
        climpd_log_e(tag, "config_new(): %s\n", strerr(errno));
        goto cleanup2;
    }
    
    for(i = 0; i < ARRAY_SIZE(handles); ++i) {
        handles[i].arg = &cc->values;
        err = config_insert_handle(&cc->config, handles + i);
        if(err < 0)
            goto cleanup3;
    }
    
    cc->values.default_playlist    = DEFAULT_VAL_DEFAULT_PLAYLIST;
    cc->values.media_active_color  = DEFAULT_VAL_MEDIA_ACTIVE_COLOR;
    cc->values.media_passive_color = DEFAULT_VAL_MEDIA_PASSIVE_COLOR;
    cc->values.volume              = DEFAULT_VAL_VOLUME;
    cc->values.repeat              = DEFAULT_VAL_REPEAT;
    cc->values.shuffle             = DEFAULT_VAL_SHUFFLE;
    
    err = climpd_config_reload(&cc->values);
    if(err < 0) {
        climpd_log_e(tag, "loading config failed: %s\n", strerr(-err));
        goto cleanup3;
    }
    
    free(config_path);
    
    climpd_log_i(tag, "initialized\n");
    
    return &cc->values;
    
cleanup3:
    config_destroy(&cc->config);
cleanup2:
    free(config_path);
cleanup1:
    free(cc);
out:
    climpd_log_e(tag, "initialization failed\n");
    errno = -err;
    return NULL;
}

void climpd_config_delete(struct climpd_config *__restrict cc)
{
    struct climpd_config_p *cc_p;
    
    cc_p = container_of(cc, struct climpd_config_p, values);
    
    config_destroy(&cc_p->config);
    free(cc_p);
    
    climpd_log_i(tag, "destroyed\n");
}

int climpd_config_reload(struct climpd_config *__restrict cc)
{
    struct climpd_config_p *cc_p;
    int err;
    
    cc_p = container_of(cc, struct climpd_config_p, values);
    
    err = config_parse(&cc_p->config);
    if(err < 0)
        return err;
    
    return 0;
}

void climpd_config_print(const struct climpd_config *__restrict cc, int fd)
{
    struct climpd_config_p *cc_p;
    struct entry *e;
    const char *key, *val;
    
    cc_p = container_of(cc, struct climpd_config_p, values);
    
    dprintf(fd, "\n climpd-config: %s\n", config_path(&cc_p->config));
    
    config_for_each(&cc_p->config, e) {
        key = entry_key(e);
        val = entry_data(e);
        
        dprintf(fd, "  %-20s = %s\n", key, val);
    }
    
    dprintf(fd, "\n");
}
