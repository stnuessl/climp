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

#include "client.h"

#define CLIMP_VERSION_MAJOR "0"
#define CLIMP_VERSION_MINOR "0"
#define CLIMP_VERSION_BUILD "2"

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
    "climp - Command Line Interface Music Player                             \n"
    "Usage: climp [option] [file]...                                         \n"
    "Options:                                                                \n"
    "\tplay          Play the file specified by the next argument.           \n"
    "\tstop          Stop climp from playing any music.                      \n"
    "\t--version     Prints the version of climp.                            \n"
    "\t-v            Same as --version.                                      \n"
};

static const char warning[] = {
    "warning: project is currently under development.\n"
    "A lot of features are still buggy or unavailable.\n"
};

int main(int argc, char *argv[])
{
    struct client *client;
    int i, err;
    
    fprintf(stdout, "%s\n", warning);
    
    client = client_new();
    if(!client) {
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
            
            err = client_request_play(client, argv[i]);
            if(err < 0)
                fprintf(stderr, "climp: play: %s\n", strerror(-err));
            
        } else if(strcmp("stop", argv[i]) == 0) {
            err = client_request_stop(client);
            if(err < 0)
                fprintf(stderr, "climp: stop: %s\n", strerror(-err));
        }
    }
    
    client_delete(client);
    
    return EXIT_SUCCESS;
}