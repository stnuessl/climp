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

#ifndef _MEDIA_LIST_LOADER_H_
#define _MEDIA_LIST_LOADER_H_

#include <stdlib.h>

#include <linux/limits.h>

#include <core/media-list.h>

struct media_list_loader {
    char *dir_path;
    char *file_path;
    size_t d_path_len;
};

int media_list_loader_init(struct media_list_loader *__restrict loader,
                           const char *__restrict path);

void media_list_loader_destroy(struct media_list_loader *__restrict loader);

int media_list_loader_load(struct media_list_loader *__restrict loader,
                           const char *__restrict path,
                           struct media_list *__restrict ml);

#endif /* _MEDIA_LIST_LOADER_H_ */