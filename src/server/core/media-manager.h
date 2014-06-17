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

#ifndef _MEDIA_MANAGER_H_
#define _MEDIA_MANAGER_H_

#include "core/media.h"

int media_manager_init(void);

void media_manager_destroy(void);

struct media *media_manager_retrieve(const char *__restrict path);

void media_manager_delete_media(const char *__restrict path);

int media_manager_parse_media(struct media *__restrict media);

void media_manager_discover_folder(const char *__restrict path, int fd);

#endif /* _MEDIA_MANAGER_H_ */