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
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/filesystem.h>
#include <libvci/error.h>

#include <core/climpd-paths.h>
#include <core/media-loader.h>
#include <core/util.h>
#include <core/climpd-log.h>

#include <obj/uri.h>

static const char *tag = "media-loader";
static char *_dir_path;
static char _file_path[PATH_MAX];
static size_t _dir_path_len;

void media_loader_init(void)
{
    const char *path = climpd_paths_media_list_loader();
    struct stat st;
    int err;
    
    _dir_path = strdup(path);
    if (!_dir_path) {
        climpd_log_e(tag, "failed to setup directory path - %s\n", errstr);
        goto fail;
    }
    
    _dir_path_len = strlen(_dir_path);

    err = stat(_dir_path, &st);
    if (err < 0) {
        if (errno != ENOENT) {
            err = -errno;
            goto fail;
        }
     
        err = path_create(_dir_path, 0755);
        if (err < 0) {
            climpd_log_e(tag, "failed to setup path \"%s\" - %s\n", _dir_path, 
                         strerr(-err));
            goto fail;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        climpd_log_e(tag, "path \"%s\" is not a directory\n", _dir_path);
        goto fail;
    }
    
    _file_path[0] = '\0';

    climpd_log_i(tag, "initialized\n");
    
    return;

fail:
    die_failed_init(tag);
}

void media_loader_destroy(void)
{
    free(_dir_path);
    
    climpd_log_i(tag, "destroyed\n");
}

int media_loader_load(const char *__restrict arg, 
                      struct media_list *__restrict ml)
{
    struct stat st;
    int err;
    
    if (uri_is_http(arg))
        return media_list_emplace_back(ml, arg);
    
    err = stat(arg, &st);
    if (err < 0) {
        if (errno != ENOENT)
            return -errno;
        
        if (_dir_path_len + strlen(arg) + 2 >= PATH_MAX) {
            climpd_log_w(tag, "encountered too long path variable\n");
            return -ENAMETOOLONG;
        }
        
        strcpy(_file_path, _dir_path);
        
        if (_file_path[_dir_path_len] != '/')
            strcat(_file_path, "/");
        
        strcat(_file_path, arg);
        
        err = stat(_file_path, &st);
        if (err < 0)
            return -errno;
        
        arg = _file_path;
    }
    
    if (!S_ISREG(st.st_mode))
        return -EMEDIUMTYPE;
    
    if (is_playlist_file(arg))
        return media_list_add_from_path(ml, arg);
    
    return media_list_emplace_back(ml, arg);
}