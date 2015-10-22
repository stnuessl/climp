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

#ifndef _MEDIA_LOADER_H_
#define _MEDIA_LOADER_H_

#include <libvci/vector.h>
#include <core/playlist/playlist.h>
#include <linux/limits.h>

struct media_loader {
    struct vector dir_vec;
    char buffer[PATH_MAX];
};

int media_loader_init(struct media_loader *__restrict ml);

void media_loader_destroy(struct media_loader *__restrict ml);

int media_loader_add_dir(struct media_loader *__restrict ml, 
                         const char *__restrict dirpath);

int media_loader_load(struct media_loader *__restrict ml, 
                      const char *__restrict arg,
                      struct playlist *__restrict playlist);

#endif /* _MEDIA_LOADER_H_ */