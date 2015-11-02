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

#ifndef _CLIMPD_CONFIG_H_
#define _CLIMPD_CONFIG_H_

#include <stdbool.h>

#include <libvci/config.h>


struct console_output_config {
    unsigned meta_column_width;
};

struct audio_player_config {
    unsigned int volume;
    float pitch;
    float speed;
    bool repeat;
    bool shuffle;
};

struct climpd_config {
    struct config conf;
    
    struct console_output_config cout_conf;
    struct audio_player_config ap_conf;

    bool keep_changes;
};

int climpd_config_init(struct climpd_config *__restrict conf,
                       const char *__restrict path);

void climpd_config_destroy(struct climpd_config *__restrict conf);

int climpd_config_load(struct climpd_config *__restrict conf);

int climpd_config_save(struct climpd_config *__restrict conf);

struct console_output_config *
climpd_config_console_output_config(struct climpd_config *__restrict conf);

struct audio_player_config *
climpd_config_audio_player_config(struct climpd_config *__restrict conf);

bool climpd_config_keep_changes(const struct climpd_config *__restrict conf);


#endif /* _CLIMPD_CONFIG_H_ */