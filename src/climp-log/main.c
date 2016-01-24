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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#define BUFFER_SIZE (1 << 12)

int main(int argc, char *argv[])
{
    static __thread char buffer[BUFFER_SIZE];
    char *path;
    bool env;
    ssize_t n;
    int fd, err;
    
    (void) argc;
    (void) argv;
    
    path = getenv("CLIMPD_LOGFILE");
    env = !!path;
    if (!path) {
        err = asprintf(&path, "/tmp/climpd-%u.log", getuid());
        if (err < 0) {
            const char *msg = strerror(errno);
            fprintf(stderr, "unable to retrieve path of log file - %s\n", msg);
            exit(EXIT_FAILURE);
        }
    }
    
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        const char *msg = strerror(errno);
        fprintf(stderr, "failed to open \"%s\" - %s\n", path, msg);
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        n = read(fd, buffer, BUFFER_SIZE);
        if (n < 0) {
            const char *msg = strerror(errno);
            fprintf(stderr, "failed to read from file - %s\n", msg);
            exit(EXIT_FAILURE);
        }
        if (n == 0)
            break;
        
        n = write(STDOUT_FILENO, buffer, n);
        if (n < 0) {
            const char *msg = strerror(errno);
            fprintf(stderr, "failed to write to stdout - %s\n", msg);
            exit(EXIT_FAILURE);
        }
    }
    
    close(fd);
    
    if (!env)
        free(path);
    
    return EXIT_SUCCESS;
}