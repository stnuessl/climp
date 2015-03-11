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

#ifndef _MEDIA_DISCOVERER_H_
#define _MEDIA_DISCOVERER_H_

#include <gst/gst.h>

#include <libvci/vector.h>

#include <core/media-list.h>

struct media_discoverer {
    GstDiscoverer *discoverer;

    char *rpath;
    char *uri;
};

int media_discoverer_init(struct media_discoverer *__restrict md);

void media_discoverer_destroy(struct media_discoverer *__restrict md);

int media_discoverer_scan_dir(struct media_discoverer *__restrict md, 
                              const char *__restrict path,
                              struct media_list *__restrict ml);

int media_discoverer_scan_all(struct media_discoverer *__restrict md, 
                              const char *__restrict path,
                              struct media_list *__restrict ml);

bool media_discoverer_file_is_playable(struct media_discoverer *__restrict md,
                                       const char *__restrict path);

#endif /* _MEDIA_DISCOVERER_H_ */