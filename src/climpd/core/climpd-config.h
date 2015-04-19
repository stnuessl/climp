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

#ifndef _CLIMPD_CONFIG_H_
#define _CLIMPD_CONFIG_H_

#include <stdbool.h>

void climpd_config_init(void);

void climpd_config_destroy(void);

int climpd_config_reload(void);

void climpd_config_print(int fd);

unsigned int climpd_config_media_meta_length(void);

const char *climpd_config_media_active_color(void);

const char *climpd_config_media_passive_color(void);

unsigned int climpd_config_volume(void);

bool climpd_config_repeat(void);

bool climpd_config_shuffle(void);

#endif /* _CLIMPD_CONFIG_H_ */