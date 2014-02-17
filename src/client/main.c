/* 
 * climp - Command Line Interface Music Player
 * Copyright (C) 2014  Steffen Nüssle
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

#include "climpd_control.h"

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
    "  previous         Play previous title.                                 \n"
    "  shutdown         Terminate the musik playing process.                 \n"
    "  stop             Stop climp from playing any music.                   \n"
    "  --version, -v    Prints the version of climp.                         \n"
    "  volume           Set volume to [0..120].                              \n"
};

int main(int argc, char *argv[])
{
    struct climpd_control *cc;
    int i, err;
    
    cc = climpd_control_new();
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
            
            if(i > argc) {
                fprintf(stderr, "climp: play: %s", strerror(ENOENT));
                continue;
            }
            
            err = climpd_control_play(cc, argv[i]);
            if(err < 0)
                fprintf(stderr, "climp: play: %s\n", strerror(-err));
            
        } else if(strcmp("stop", argv[i]) == 0) {
            err = climpd_control_stop(cc);
            if(err < 0)
                fprintf(stderr, "climp: stop: %s\n", strerror(-err));
            
        } else if(strcmp("volume", argv[i]) == 0) {
            i += 1;
            if(i > argc) {
                fprintf(stderr, "climp: volume: %s\n", strerror(EINVAL));
                continue;
            }
            
            err = climpd_control_set_volume(cc, argv[i]);
            if(err < 0)
                fprintf(stderr, "climp: volume: %s\n", strerror(-err));
            
        } else if(strcmp("mute", argv[i]) == 0) {
            err = climpd_control_toggle_mute(cc);
            if(err < 0)
                fprintf(stderr, "climp: mute: %s\n", strerror(-err));
            
        } else if(strcmp("shutdown", argv[i]) == 0) {
            err = climpd_control_shutdown(cc);
            if(err < 0)
                fprintf(stderr, "climp: shutdown: %s\n", strerror(-err));
            
        } else if(strcmp("add", argv[i]) == 0) {
            i += 1;
            
            if(i >= argc) {
                fprintf(stderr, "climp: add: %s\n", strerror(ENOENT));
                continue;
            }
            
            err = climpd_control_add(cc, argv[i]);
            if(err < 0)
                fprintf(stderr, "climp: add: %s\n", strerror(-err));
            
        } else if(strcmp("next", argv[i]) == 0) {
            err = climpd_control_next(cc);
            if(err < 0)
                fprintf(stderr, "climp: next: %s\n", strerror(-err));
            
        } else if(strcmp("previous", argv[i]) == 0) {
            err = climpd_control_previous(cc);
            if(err < 0)
                fprintf(stderr, "climp: previous: %s\n", strerror(-err));
            
        }
    }
    
    climpd_control_delete(cc);
    
    return EXIT_SUCCESS;
}