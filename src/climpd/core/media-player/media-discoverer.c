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

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/map.h>
#include <libvci/compare.h>
#include <libvci/hash.h>

#include <core/media-player/media-discoverer.h>

static const char *tag = "media-discoverer";

static struct map _media_map;
static GstDiscoverer _discoverer;

int media_discoverer_init(void)
{
    
}

void media_discoverer_destroy(void)
{
    media_discoverer_stop();
    
    map_destroy(&_media_map);
}

void media_discoverer_start(void)
{
    
}

void media_discoverer_stop(void)
{
    
}

void media_discoverer_discover(struct media *m)
{
    
}