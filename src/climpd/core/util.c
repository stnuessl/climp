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
#include <stdarg.h>
#include <errno.h>

#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/util.h>

void die(const char *__restrict tag, const char *__restrict msg)
{
    climpd_log_e(tag, "%s\n", msg);
    exit(EXIT_FAILURE);
}

void die_errno(const char *__restrict tag, const char *__restrict msg, int err)
{
    climpd_log_e(tag, "%s - %s\n", msg, strerr(abs(err)));
    exit(EXIT_FAILURE);
}

void die_fmt(const char *__restrict tag, const char *__restrict fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    climpd_log_v_e(tag, fmt, vargs);
    
    va_end(vargs);
    
    exit(EXIT_FAILURE);
}

void die_failed_init(const char *__restrict tag)
{
    die(tag, "failed to initialize - aborting...");
}

const char *yes_no(bool val)
{
    return (val) ? "yes" : "no";
}

const char *true_false(bool val)
{
    return (val) ? "true" : "false";
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
    if (!strcasecmp(s, "true") || !strcasecmp(s, "yes") || !strcasecmp(s, "on") 
        || !strcasecmp(s, "y") || !strcasecmp(s, "1")) {
        *val = true;
        return 0;
    } 
    
    if (!strcasecmp(s, "false") || !strcasecmp(s, "no") || !strcasecmp(s, "off")
        || !strcasecmp(s, "n")  || !strcasecmp(s, "0")) {
        *val = false;
        return 0;
    }
    
    return -EINVAL;
}