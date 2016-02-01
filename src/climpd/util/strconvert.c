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
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <util/strconvert.h>

bool str_is_int(const char *__restrict s)
{
    if (*s == '+' || *s == '-')
        ++s;
    
    while (*s != '\0') {
        if (!isxdigit(*s++))
            return false;
    }
    
    return true;
}

int str_to_int(const char *__restrict s, int *__restrict i)
{
    char *end;
    int val;
    
    errno = 0;
    val = (int) strtol(s, &end, 10);
    
    if (errno != 0)
        return -errno;
    
    if (*end != '\0')
        return -EINVAL;
    
    *i = val;
    
    return 0;
}

int str_to_float(const char *__restrict s, float *f)
{
    char *end;
    float val;
    
    errno = 0;
    val = strtof(s, &end);
    if (errno)
        return -errno;
    
    if (*end != '\0')
        return -EINVAL;
    
    *f = val;
    
    return 0;
}

int str_to_sec(const char *__restrict s, int *__restrict sec)
{
    char *r;
    int val;
    
    errno = 0;
    val = strtol(s, &r, 10);
    if (errno)
        return -errno;
    
    if (*r != '\0') {
        if (*r != ':' && *r != '.' && *r != ',' && *r != ' ')
            return -EINVAL;
        
        val *= 60;
        errno = 0;
        
        val += strtol(r + 1, &r, 10);
        if (errno)
            return -errno;
        else if (*r != '\0')
            return -EINVAL;
    }
    
    *sec = val;
    
    return 0;
}

int str_to_bool(const char *__restrict s, bool *__restrict val)
{
    if (strcasecmp(s, "true") == 0 || 
        strcasecmp(s, "yes") == 0  || 
        strcasecmp(s, "on") == 0   ||
        strcasecmp(s, "y") == 0    ||
        strcmp(s, "1") == 0) {
        *val = true;
        return 0;
    } 
    
    if (strcasecmp(s, "false") == 0 ||
        strcasecmp(s, "no") == 0    ||
        strcasecmp(s, "off") == 0   ||
        strcasecmp(s, "n") == 0     ||
        strcmp(s, "0") == 0) {
        *val = false;
        return 0;
    }
    
    return -EINVAL;
}