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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <libvci/map.h>
#include <libvci/macro.h>

#include "climpd.h"

#define CLIMP_VERSION_MAJOR "0"
#define CLIMP_VERSION_MINOR "0"
#define CLIMP_VERSION_MICRO "4"


const char *climp_version(void)
{
    return CLIMP_VERSION_MAJOR"."CLIMP_VERSION_MINOR"."CLIMP_VERSION_MICRO;
}

static const char license[] = {
    "climp - Command Line Interface Music Player                             \n"
    "Copyright (C) 2014  Steffen Nüssle                                    \n\n"
    "This program is free software: you can redistribute it and/or modify    \n"
    "it under the terms of the GNU General Public License as published by    \n"
    "the Free Software Foundation, either version 3 of the License, or       \n"
    "(at your option) any later version.                                   \n\n"
    "This program is distributed in the hope that it will be useful,         \n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of          \n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           \n"
    "GNU General Public License for more details.                            \n"
    "You should have received a copy of the GNU General Public License       \n"
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.   \n"
};

static const char help[] = {
    "climp - Command Line Interface Music Player                           \n\n"
    "Usage: climp [option] [file] [0..120]                                   \n"
    "Options:                                                              \n\n"
    "  add              Add [file] to the playlist.                          \n"
    "  --help           Print this text.                                     \n"
    "  mute             Toggle mute / unmute.                                \n"
    "  next             Play next title.                                     \n"
    "  play             Play [file].                                         \n"
    "  playlist         Prints to current playlist.                          \n"
    "  previous         Play previous title.                                 \n"
    "  shutdown         Terminate the musik playing process.                 \n"
    "  stop             Stop climp from playing any music.                   \n"
    "  --version, -v    Prints the version of climp.                         \n"
    "  volume           Set volume to [0..120].                              \n"
};

static int string_compare(const void *a, const void *b)
{
    return strcmp(a, b);
}

static unsigned int string_hash(const void *key)
{
    const char *s;
    unsigned int hval;
    size_t len;
    
    s = key;
    hval = 1;
    len  = strlen(s);
    
    while(len--) {
        hval += *s++;
        hval += (hval << 10);
        hval ^= (hval >> 6);
        hval &= 0x0fffffff;
    }
    
    hval += (hval << 3);
    hval ^= (hval >> 11);
    hval += (hval << 15);
    
    return hval;
}

struct command_handle {
    const char *command;
    
    void (*arg_func)(struct climpd *, const char *);
    void (*func)(struct climpd *);
};

/*
 * get-playlist
 * get-files
 * get-volume
 * get-status
 * 
 * set-status [play / stop / pause / mute / unmute]
 * set-playlist [file1 file2 file3]
 * set-volume [0..120]
 * 
 * play-next
 * play-previous
 * play-title [file1 file2 file3]
 * play-number [number]
 * 
 * add-file
 * remove-file
 */


static struct command_handle command_handles[] = {
    { "get-playlist",    NULL,                   climpd_get_playlist     },
    { "get-files",       NULL,                   climpd_get_files        },
    { "get-volume",      NULL,                   climpd_get_volume       },
    { "set-status",      climpd_set_status,      NULL                    },
    { "set-playlist",    climpd_set_playlist,    NULL                    },
    { "set-volume",      climpd_set_volume,      NULL                    },
    { "play-next",       NULL,                   climpd_play_next        },
    { "play-previous",   NULL,                   climpd_play_previous    },
    { "play-file",       climpd_play_file,       NULL                    },
    { "play-track",      climpd_play_track,      NULL                    },
    { "add-media",       climpd_add_media,       NULL                    },
    { "add-playlist",    climpd_add_playlist,    NULL                    },
    { "remove-media",    climpd_remove_media,    NULL                    },
    { "remove-playlist", climpd_remove_playlist, NULL                    }
};

static struct map *command_map;

void command_map_init(void)
{
    struct command_handle *handle;
    int i, err;
    
    command_map = map_new(0, &string_compare, &string_hash);
    if(!command_map) {
        fprintf(stderr, "climp: %s\n", strerror(ENOMEM));
        exit(EXIT_FAILURE);
    }
    
    for(i = 0; i < ARRAY_SIZE(command_handles); ++i) {
        handle = command_handles + i;
        
        err = map_insert(command_map, handle->command, handle);
        if(err < 0) {
            map_delete(command_map);
            fprintf(stderr, "climp: %s\n", strerror(-err));
            exit(EXIT_FAILURE);
        }
    }
}

void command_map_destroy(void)
{
    map_delete(command_map);
}

int main(int argc, char *argv[])
{
    struct climpd *climpd;
    struct command_handle *h, *tmp;
    const char *err_msg;
    int i;
    
    if(getuid() == 0) {
        fprintf(stderr, "climp: run as root\n");
        exit(EXIT_FAILURE);
    }
    
    command_map_init();
    
    climpd = climpd_new();
    if(!climpd) {
        fprintf(stderr, "climp: client_new(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
        
    for(i = 1; i < argc; ++i) {
        if(strcmp("--version", argv[i]) == 0 || strcmp("-v", argv[i]) == 0) {
            fprintf(stdout, "%s\nVersion %s\n\n", license, climp_version());
            
        } else if(strcmp("--help", argv[i]) == 0) {
            fprintf(stdout, "%s\n", help);
            
        } else {
            h = map_retrieve(command_map, argv[i]);
            if(!h) {
                err_msg = strerror(EINVAL);
                fprintf(stderr, "climp: %s - %s\n", argv[i], err_msg);
                continue;
            }
            
            if(h->func) {
                h->func(climpd);
                continue;
            }

            i += 1;
            
            if(i >= argc) {
                err_msg = strerror(EINVAL);
                fprintf(stderr, "climp: %s: %s\n", h->command, err_msg);
                continue;
            }
            
            while(i < argc) {
                /* check for command arguments until a new one is found */
                tmp = map_retrieve(command_map, argv[i]);
                if(tmp) {
                    i -= 1;
                    break;
                }
                
                h->arg_func(climpd, argv[i]);
                
                i += 1;
            }
        }
    }
    
    climpd_delete(climpd);
    command_map_destroy();
    
    return EXIT_SUCCESS;
}