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
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <libvci/filesystem.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/media-loader.h>
#include <media/uri.h>

static const char *tag = "media-loader";

static bool is_playlist_file(const char *__restrict path)
{
    const char *p = strrchr(path, '.');
    
    return (p) ? strcmp(p, ".m3u") == 0 || strcmp(p, ".txt") == 0 : false; 
}

static int pathcat(char *__restrict dst,
                   size_t size,
                   const char *__restrict src1, 
                   const char *__restrict src2)
{
    size_t len1, len2;
    
    len1 = strlen(src1);
    len2 = strlen(src2);
    
    if (len1 + len2  + 2 >= size)
        return -ENAMETOOLONG;   
    
    strcpy(dst, src1);
    
    if (src1[len1 - 1] != '/')
        strcat(dst, "/");
    
    strcat(dst, src2);
    
    return 0;
}

static const char *find_file(struct media_loader *__restrict ml, 
                             const char *arg)
{
    unsigned int size;

    size = vector_size(&ml->dir_vec);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct stat st;
        const char *ele;
        int err;
        
        ele = *vector_at(&ml->dir_vec, i);
        
        err = pathcat(ml->buffer, sizeof(ml->buffer), ele, arg);
        if (err < 0)
            continue;
        
        err = stat(ml->buffer, &st);
        if (err < 0)
            continue;
        
        if (S_ISREG(st.st_mode))
            return ml->buffer;
    }
    
    return NULL;
}

int media_loader_init(struct media_loader *__restrict ml)
{
    int err = vector_init(&ml->dir_vec, 0);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize - %s\n", strerr(-err));
        return err;
    }
    
    vector_set_data_delete(&ml->dir_vec, &free);
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;
}

void media_loader_destroy(struct media_loader *__restrict ml)
{
    vector_destroy(&ml->dir_vec);
    
    climpd_log_i(tag, "destroyed\n");
}

int media_loader_add_dir(struct media_loader *__restrict ml,
                         const char *__restrict dirpath)
{
    char *dup;
    int err;
    
    dup = strdup(dirpath);
    if (!dup) {
        err = -errno;
        goto fail;
    }
    
    err = vector_insert_back(&ml->dir_vec, dup);
    if (err < 0) {
        free(dup);
        goto fail;
    }

    climpd_log_i(tag, "added directory '%s'\n", dup);
    
    return 0;
    
fail:
    climpd_log_e(tag, "failed to add '%s' - %s\n", dirpath, strerr(-err));
    
    return err;
}

int media_loader_load(struct media_loader *__restrict ml, 
                      const char *__restrict arg,
                      struct playlist *__restrict playlist)
{
    const char *path;
    int err;
    
    if (uri_is_http(arg)) {
        err = playlist_add(playlist, arg);
        if (err < 0) {
            climpd_log_e(tag, "failed to load web source '%s' - %s\n", arg, 
                         strerr(-err));
            return err;
        }
        
        climpd_log_i(tag, "loaded web source '%s'\n", arg);
        return 0;
    }
    
    if (uri_is_file(arg))
        arg = uri_hierarchical(arg);

    if (path_is_reg(arg)) {
        if (is_playlist_file(arg)) {
            err = playlist_load(playlist, arg);
            if (err < 0) {
                climpd_log_e(tag, "failed to load playlist '%s' - %s\n", arg,
                             strerr(-err));
                return err;
            }
            
            climpd_log_i(tag, "loaded playlist '%s'\n", arg);
        } else {
            err = playlist_add(playlist, arg);
            if (err < 0) {
                climpd_log_e(tag, "failed to load file '%s' - %s\n", arg, 
                             strerr(-err));
                return err;
            }
            
            climpd_log_i(tag, "loaded file '%s'\n", arg);
        }
        
        return 0;
    }
    
    path = find_file(ml, arg);
    if (!path) {
        climpd_log_e(tag, "failed to locate file '%s' - %s\n", arg, 
                     strerr(-ENOENT));
        return -ENOENT;
    }
    
    climpd_log_i(tag, "file '%s' is located at '%s'\n", arg, path);
    
    return media_loader_load(ml, path, playlist);
}