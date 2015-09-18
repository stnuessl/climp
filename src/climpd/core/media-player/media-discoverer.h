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

#ifndef _MEDIA_DISCOVERER_H_
#define _MEDIA_DISCOVERER_H_

#include <obj/media.h>
#include <obj/media-list.h>

int media_discoverer_init(void);

void media_discoverer_destroy(void);

int media_discoverer_discover_media_sync(struct media* m);

void media_discoverer_discover_media_async(struct media *m);

void media_discoverer_discover_media_list_async(struct media_list *ml);

#endif /* _MEDIA_DISCOVERER_H_ */