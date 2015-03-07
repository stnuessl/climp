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

#include <libvci/error.h>

#include "../shared/ipc.h"
#include "climpd.h"

int main(int argc, char *argv[])
{
    int err;
    
    if(getuid() == 0) {
        fprintf(stderr, "climp: run as root\n");
        exit(EXIT_FAILURE);
    }

    err = climpd_init();
    if(err < 0) {
        fprintf(stderr, "climp: climpd_init(): %s\n", strerror(-err));
        exit(EXIT_FAILURE);
    }
    
    climpd_handle_args(argc - 1, (const char **) argv + 1);
    
    climpd_destroy();
    
    return EXIT_SUCCESS;
}