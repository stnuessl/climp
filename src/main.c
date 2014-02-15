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

#include <libvci/macro.h>

#include "climp_player.h"

#define CLIMP_VERSION_MAJOR "0"
#define CLIMP_VERSION_MINOR "0"
#define CLIMP_VERSION_BUILD "1"

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

int main(int argc, char *argv[])
{
    int i, err;
    const char *title;
    
    for(i = 0; i < argc; ++i) {
        
        if(strcmp("--version", argv[i]) == 0 || strcmp("-v", argv[i]) == 0) {
            fprintf(stdout, "%s\nVersion %s\n\n", license, climp_version());
        } else if(strcmp("--play", argv[i]) == 0) {
            if(i + 1 >= argc) {
                fprintf(stderr, "climp: error: no title specified.\n");
                exit(EXIT_FAILURE);
            }
            
            title = argv[i + 1];
            
            err = climp_player_init();
            if(err  < 0) {
                fprintf(stderr, "climp: error: initializing player failed.\n");
                exit(EXIT_FAILURE);
            }
            
            err = climp_player_send_message("Hello climp!");
            if(err < 0) {
                fprintf(stderr, "climp: error: sending message failed.\n");
                exit(EXIT_FAILURE);
            }
            
            err = climp_player_play_title(title);
            if(err < 0) {
                fprintf(stderr, "climp: error: playing title failed.\n");
                exit(EXIT_FAILURE);
            }
            
            climp_player_handle_events();
        }
    }
    
    return EXIT_SUCCESS;
}