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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libvci/macro.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/climpd-config.h>
#include <util/bool.h>
#include <util/strconvert.h>

static const char *tag = "climpd-config";

static void log_invalid_value(const char *__restrict key, 
                              const char *__restrict val,
                              int err)
{
    climpd_log_w(tag, "invalid '%s' value: '%s' - %s\n", key, val, strerr(err));
}

static void parse_meta_column_width(const char *key, const char *val, void *arg)
{
    struct climpd_config *conf = arg;
    int col_width, err;
    
    err = str_to_int(val, &col_width);
    if (err < 0) {
        log_invalid_value(key, val, -err);
        return;
    }
    
    conf->cout_conf.meta_column_width = (unsigned int) col_width;
    
    climpd_log_i(tag, "'%s' -> '%u'\n", key, conf->cout_conf.meta_column_width);
}

static void parse_volume(const char *key, const char *val, void *arg)
{
    struct climpd_config *conf = arg;
    int vol, err;
    
    err = str_to_int(val, &vol);
    if (err < 0) {
        log_invalid_value(key, val, -err);
        return;
    }
    
    vol = min(vol, 100);
    vol = max(vol, 0);
    
    conf->ap_conf.volume = (unsigned int) vol;
    
    climpd_log_i(tag, "'%s' -> '%u'\n", key, conf->ap_conf.volume);
}

static void parse_pitch(const char *key, const char *val, void *arg)
{
    struct climpd_config *conf = arg;
    float pitch;
    int err;
    
    err = str_to_float(val, &pitch);
    if (err < 0) {
        log_invalid_value(key, val, -err);
        return;
    }
    
    pitch = min(pitch, 10.0f);
    pitch = max(pitch, 0.1f);
    
    conf->ap_conf.pitch = pitch;
    
    climpd_log_i(tag, "'%s' -> '%f'\n", key, conf->ap_conf.pitch);
}

static void parse_speed(const char *key, const char *val, void *arg)
{
    struct climpd_config *conf = arg;
    float speed;
    int err;
    
    err = str_to_float(val, &speed);
    if (err < 0) {
        log_invalid_value(key, val, -err);
        return;
    }
    
    speed = min(speed, 40.0f);
    speed = max(speed, 0.1f);
    
    conf->ap_conf.speed = speed;
    
    climpd_log_i(tag, "'%s' -> '%f'\n", key, conf->ap_conf.speed);
}

static void parse_repeat(const char *key, const char *val, void *arg)
{
    struct climpd_config *conf = arg;
    bool repeat;
    int err;
    
    err = str_to_bool(val, &repeat);
    if (err < 0) {
        log_invalid_value(key, val, -err);
        return;
    }
    
    conf->ap_conf.repeat = repeat;
    
    climpd_log_i(tag, "'%s' -> '%s'\n", key, yes_no(conf->ap_conf.repeat));
}

static void parse_shuffle(const char *key, const char *val, void *arg)
{
    struct climpd_config *conf = arg;
    bool shuffle;
    int err;
    
    err = str_to_bool(val, &shuffle);
    if (err < 0) {
        log_invalid_value(key, val, -err);
        return;
    }
    
    conf->ap_conf.shuffle = shuffle;
    
    climpd_log_i(tag, "'%s' -> '%s'\n", key, yes_no(conf->ap_conf.shuffle));
}

static void parse_keep_changes(const char *key, const char *val, void *arg)
{
    struct climpd_config *conf = arg;
    bool keep;
    int err;
    
    err = str_to_bool(val, &keep);
    if (err < 0) {
        log_invalid_value(key, val, -err);
        return;
    }
    
    conf->keep_changes = keep;
    
    climpd_log_i(tag, "'%s' -> '%s'\n", key, yes_no(conf->keep_changes));
}

static void write_config(int fd, void *arg)
{
    struct climpd_config *conf = arg;
    
    dprintf(fd,
            "# climpd configuration\n#\n"
            "# Valid Ranges for\n"
            "# - Volume : [0, 100]\n"
            "# - Pitch  : [0.1, 10.0]\n"
            "# - Speed  : [0.1, 40.0]\n#\n\n"
            "# Column width for media meta information\n"
            "ConsoleOutput.Meta_Column_Width = %u\n\n"
            "# Player Settings\n"
            "AudioPlayer.Volume = %u\n"
            "AudioPlayer.Pitch = %.2f\n"
            "AudioPlayer.Speed = %.2f\n"
            "AudioPlayer.Repeat = %s\n"
            "AudioPlayer.Shuffle = %s\n\n"
            "# Config options\n"
            "Config.Keep_Changes = %s\n\n",
            conf->cout_conf.meta_column_width, conf->ap_conf.volume, 
            conf->ap_conf.pitch, conf->ap_conf.speed, 
            yes_no(conf->ap_conf.repeat), yes_no(conf->ap_conf.shuffle), 
            yes_no(conf->keep_changes));
}

static struct config_handle handles[] = {
    { &parse_meta_column_width, "ConsoleOutput.Meta_Column_Width", NULL },
    { &parse_volume,            "AudioPlayer.Volume",              NULL },
    { &parse_pitch,             "AudioPlayer.Pitch",               NULL },
    { &parse_speed,             "AudioPlayer.Speed",               NULL },
    { &parse_repeat,            "AudioPlayer.Repeat",              NULL },
    { &parse_shuffle,           "AudioPlayer.Shuffle",             NULL },
    { &parse_keep_changes,      "Config.Keep_Changes",             NULL },
};

int climpd_config_init(struct climpd_config *__restrict conf, 
                       const char *__restrict path)
{
    int err;
    
    /* Initialize config with sane defaults */
    conf->cout_conf.meta_column_width = 24;
    conf->ap_conf.volume = 60;
    conf->ap_conf.pitch = 1.0f;
    conf->ap_conf.speed = 1.0f;
    conf->ap_conf.repeat = true;
    conf->ap_conf.shuffle = false;
    conf->keep_changes = false;
    
    err = config_init(&conf->conf, path, &write_config, conf);
    if (err < 0)
        goto out;
    
    for(unsigned int i = 0; i < ARRAY_SIZE(handles); ++i) {
        /* TODO: config handle args are a mess... */
        handles[i].arg = conf;
        
        err = config_insert_handle(&conf->conf, handles + i);
        if(err < 0) {
            climpd_log_e(tag, "adding config handle \"%s\" failed - %s\n",
                         handles[i].key, strerr(-err));
            goto cleanup1;
        }
    }
    
    err = climpd_config_load(conf);
    if (err < 0)
        goto cleanup1;
    
    climpd_log_i(tag, "initialized configuration '%s'\n", path);
    
    return 0;

cleanup1:
    config_destroy(&conf->conf);
out:
    climpd_log_e(tag, "failed to initialize configuration file '%s' - %s\n",
                 path, strerr(-err));
    return err;
}

void climpd_config_destroy(struct climpd_config *__restrict conf)
{
    config_destroy(&conf->conf);
    
    climpd_log_i(tag, "destroyed\n");
}

int climpd_config_load(struct climpd_config *__restrict conf)
{
    int err = config_parse(&conf->conf);
    if (err < 0) {
        climpd_log_e(tag, "failed to load configuration - %s\n", strerr(-err));
        return err;
    }
    
    climpd_log_i(tag, "loaded configuration\n");
    return 0;
}

int climpd_config_save(struct climpd_config *__restrict conf)
{
    int fd = open(config_path(&conf->conf), O_WRONLY | O_TRUNC);
    if (fd < 0) {
        int err = -errno;
        climpd_log_e(tag, "failed to open config for writing - %s\n", errstr);
        return err;
    }
    
    write_config(fd, conf);
    
    close(fd);
    
    climpd_log_i(tag, "saved current configuration\n");
    
    return 0;
}

struct console_output_config *
climpd_config_console_output_config(struct climpd_config *__restrict conf)
{
    return &conf->cout_conf;
}

struct audio_player_config *
climpd_config_audio_player_config(struct climpd_config *__restrict conf)
{
    return &conf->ap_conf;
}

bool climpd_config_keep_changes(const struct climpd_config *__restrict conf)
{
    return conf->keep_changes;
}