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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../climpd/core/climpd-log.h"
#include "../climpd/core/playlist/playlist.h"

int main(int argc, char *argv[])
{
    struct playlist playlist;
    struct media *m1, *m2;
    
    gst_init(NULL, NULL);
    climpd_log_init();

    assert(playlist_init(&playlist) == 0 && "playlist_init");
    
    for (int i = 1; i < argc; ++i) {
        if (strstr(argv[i], ".txt") || strstr(argv[i], ".m3u"))
            assert(playlist_load(&playlist, argv[i]) == 0 && "playlist_load");
        else
            assert(playlist_add(&playlist, argv[i]) == 0 && "playlist_add");
    }
    
    printf("---- Playlist ----\n");
    
    for (unsigned int i = 0, size = playlist_size(&playlist); i < size; ++i) {
        struct media *m = playlist_at(&playlist, i);
        struct media_info *info = media_info(m);
        
        printf(" ( %2u ) %-20s %-20s %s\n", i + 1, info->title, info->album, 
               info->artist);
        
        /* internal tag reader, playlist and the reference here */
        assert(m->ref_count == 3 && "invalid media reference count");
        
        media_unref(m);
    }
    
    if (!playlist_empty(&playlist)) {
        m1 = playlist_at(&playlist, -1);
        m2 = playlist_at(&playlist, playlist_size(&playlist) - 1);
        
        assert(m1 == m2 && "invalid last element match");
        
        media_unref(m2);
        media_unref(m1);
    }
        
    playlist_destroy(&playlist);
    climpd_log_destroy();
    gst_deinit();
    
    return 0;
}