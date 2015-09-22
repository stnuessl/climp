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
#include <stdbool.h>
#include <linux/limits.h>

#include <libvci/filesystem.h>

#include <media/uri.h>

char *uri_new(const char *__restrict arg)
{
    static __thread char rpath[PATH_MAX];
    static const char file_scheme[] = "file://";
    char *uri;
    size_t len;
    
    if (uri_ok(arg))
        return strdup(arg);
    
    /* if no valid uri was passed we assume 'arg' points to a local file */
    arg = realpath(arg, rpath);
    if (!arg)
        return NULL;
    
    len = strlen(arg);
    
    uri = malloc(sizeof(file_scheme) + len);
    if (!uri)
        return NULL;
    
    strcpy(uri, file_scheme);
    strcat(uri, arg);
    
    return uri;
}

void uri_delete(char *__restrict uri)
{
    free(uri);
}

const char *uri_hierarchical(const char *__restrict uri)
{
    return strstr(uri, "://") + sizeof("://") - 1;
}

bool uri_is_file(const char *__restrict uri)
{
    const char *p = strstr(uri, "file://");
    
    return (p) ? path_is_absolute(p + sizeof("file://") - 1) : false;
}

bool uri_is_http(const char *__restrict uri)
{
    return strstr(uri, "http://") != NULL || strstr(uri, "https://") != NULL;
}

bool uri_ok(const char *__restrict uri)
{
    return uri_is_file(uri) || uri_is_http(uri);
}