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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>

const char* strerr(int err)
{
    static __thread char s[64];
    
    return strerror_r(err, s, sizeof(s));
}

int create_leading_directories(const char *path, mode_t mode)
{
    struct stat st;
    char *path_dup, *p;
    int err;
    
    path_dup = strdup(path);
    if(!path_dup)
        return -errno;

    p = path_dup;
    
    while(*p == '/')
        p += 1;
    
    for(p = strchrnul(p, '/'); *p != '\0'; p = strchrnul(p + 1, '/')) {
        
        while(*++p == '/')
            ;
        
        *--p = '\0';
        
        err = stat(path_dup, &st);
        if(err < 0) {
            if(errno != ENOENT) {
                err = -errno;
                goto fail;
            }
        } else {
            if(S_ISDIR(st.st_mode)) {
                *p = '/';
                continue;
            }
        }

        err = mkdir(path_dup, mode);
        
        *p++ = '/';
        
        if(err < 0) {
            if(errno != EEXIST) {
                err = -errno;
                goto fail;
            }
        }
    }
    
    free(path_dup);
    
    return 0;
    
fail:
    free(path_dup);
    return err;
}

int is_file(const char *path, bool *ret)
{
    struct stat s;
    int err;
    
    err = stat(path, &s);
    if(err < 0)
        return -errno;
    
    *ret = S_ISREG(s.st_mode);
    
    return 0;
}

int is_dir(const char *path, bool *ret)
{
    struct stat s;
    int err;
    
    err = stat(path, &s);
    if(err < 0)
        return -errno;
    
    *ret = S_ISDIR(s.st_mode);
    
    return 0;
}