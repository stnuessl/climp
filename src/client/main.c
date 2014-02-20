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

#include "climpd.h"

#define CLIMP_VERSION_MAJOR "0"
#define CLIMP_VERSION_MINOR "0"
#define CLIMP_VERSION_BUILD "3"


const char *climp_version(void)
{
    return CLIMP_VERSION_MAJOR"."CLIMP_VERSION_MINOR"."CLIMP_VERSION_BUILD;
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

int main(int argc, char *argv[])
{
    struct climpd *cc;
    char *arg;
    int i;
    
    if(getuid() == 0) {
        fprintf(stderr, "climp: run as root\n");
        exit(EXIT_FAILURE);
    }
    
    cc = climpd_new();
    if(!cc) {
        fprintf(stderr, "climp: client_new(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
        
    for(i = 0; i < argc; ++i) {
        if(strcmp("--version", argv[i]) == 0 || strcmp("-v", argv[i]) == 0) {
            fprintf(stdout, "%s\nVersion %s\n\n", license, climp_version());
            
        } else if(strcmp("--help", argv[i]) == 0) {
            fprintf(stdout, "%s\n", help);
            
        } else if(strcmp("play", argv[i]) == 0) {
            i += 1;
            
            if(i >= argc)
                arg = NULL;
            else 
                arg = argv[i];
            
            climpd_play(cc, arg);
        } else if(strcmp("stop", argv[i]) == 0) {
            climpd_stop(cc);
            
        } else if(strcmp("volume", argv[i]) == 0) {
            i += 1;
            if(i >= argc) {
                fprintf(stderr, "climp: volume: %s\n", strerror(EINVAL));
                continue;
            }
            
            climpd_set_volume(cc, argv[i]);
        } else if(strcmp("mute", argv[i]) == 0) {
            climpd_toggle_mute(cc);
     
            
        } else if(strcmp("shutdown", argv[i]) == 0) {
            climpd_shutdown(cc);
            
        } else if(strcmp("add", argv[i]) == 0) {
            i += 1;
            
            if(i >= argc) {
                fprintf(stderr, "climp: add: %s\n", strerror(ENOENT));
                continue;
            }
            
            climpd_add(cc, argv[i]);
            
        } else if(strcmp("next", argv[i]) == 0) {
            climpd_next(cc);
            
        } else if(strcmp("previous", argv[i]) == 0) {
            climpd_previous(cc);
            
        } else if(strcmp("playlist", argv[i]) == 0) {
            i += 1;
            
            if(i >= argc)
                arg = NULL;
            else
                arg = argv[i];
            
            climpd_playlist(cc, arg);
        }
    }
    
    climpd_delete(cc);
    
    return EXIT_SUCCESS;
}