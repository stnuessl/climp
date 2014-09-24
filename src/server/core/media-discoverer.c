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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/error.h>

#include "core/media-discoverer.h"

#include "util/climpd-log.h"

static const char *tag = "media-discoverer";

static bool is_playable(struct media_discoverer *__restrict md,
                        const char *__restrict uri)
{
    GstDiscovererResult result;
    GstDiscovererInfo *info;
    GstDiscovererStreamInfo *s_info, *s_info_next;
    GError *err;
    
    info = gst_discoverer_discover_uri(md->discoverer, uri, &err);
    
    if(err)
        g_error_free(err);
    
    if(!info)
        return false;
    
    result = gst_discoverer_info_get_result(info);
    
    if(result != GST_DISCOVERER_OK) {
        gst_discoverer_info_unref(info);
        return false;
    } 
    
    s_info = gst_discoverer_info_get_stream_info(info);
    
    while(s_info) {
        if(GST_IS_DISCOVERER_VIDEO_INFO(s_info)) {
            gst_discoverer_stream_info_unref(s_info);
            gst_discoverer_info_unref(info);
            return false;
        }
        
        s_info_next = gst_discoverer_stream_info_get_next(s_info);
        
        gst_discoverer_stream_info_unref(s_info);
        
        s_info = s_info_next;
    }

    gst_discoverer_info_unref(info);
    
    return true;
}

int media_discoverer_init(struct media_discoverer *__restrict md, 
                          const char *path,
                          int fd)
{
    GError *error;
    char *path_dup;
    size_t len;
    int err;
    
    md->discoverer = gst_discoverer_new(5 * GST_SECOND, &error);
    if(!md->discoverer) {
        err = -error->code;
        climpd_log_e(tag, "creating discoverer failed - %s\n", error->message);
        g_error_free(error);
        return err;
    }
    
    md->dir_vec = vector_new(64);
    if(!md->dir_vec) {
        err = -errno;
        goto cleanup1;
    }
    
    vector_set_data_delete(md->dir_vec, &free);
    
    len = strlen(path);
    
    path_dup = malloc(len + 1 + 1);
    if(!path_dup) {
        err = -errno;
        goto cleanup2;
    }
    
    path_dup = strcpy(path_dup, path);
    
    if(path[len] != '/')
        path_dup = strcat(path_dup, "/");
    
    err = vector_insert_back(md->dir_vec, path_dup);
    if(err < 0)
        goto cleanup3;
    
    md->fd = fd;
    
    return 0;
    
cleanup3:
    free(path_dup);
cleanup2:
    vector_delete(md->dir_vec);
cleanup1:
    gst_object_unref(md->discoverer);
    
    return err;
}

void media_discoverer_destroy(struct media_discoverer *__restrict md)
{
    vector_delete(md->dir_vec);
    gst_object_unref(md->discoverer);
}


void media_discoverer_scan(struct media_discoverer *__restrict md)
{
    DIR *dir;
    struct dirent buf, *ent;
    char *current_dir, *path, *uri;
    size_t len, len_path;
    unsigned int i;
    int err;
    
    /* 
     * Don't use 'vector_for_each()', the iterator pointer may become invalid
     * if the vector gets resized during a insert-operation. Yes, I insert
     * elements into the vector while iterating.
     */
    for(i = 0; i < vector_size(md->dir_vec); ++i) {
        current_dir = *vector_at(md->dir_vec, i);
        
        len_path = strlen(current_dir);
        
        dir = opendir(current_dir);
        if(!dir) {
            climpd_log_w(tag, "failed to open directory");
            climpd_log_append(" %s - %s\n", current_dir, errstr);
            continue;
        }
        
        while(1) {
            err = readdir_r(dir, &buf, &ent);
            if(err || !ent)
                break;
            
            if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            
            if(ent->d_type == DT_DIR) {
                path = malloc(len_path + strlen(ent->d_name) + 2);
                if(!path) {
                    err = -errno;
                    climpd_log_w(tag, "failed to open directory %s - %s\n", 
                                 ent->d_name, strerr(-err));
                    continue;
                }
                
                path = strcpy(path, current_dir);
                path = strcat(path, ent->d_name);
                path = strcat(path, "/");
                
                err = vector_insert_back(md->dir_vec, path);
                if(err < 0) {
                    climpd_log_w(tag, "skipping directory %s - %s\n", path, 
                                 strerr(-err));
                    continue;
                }
                
            } else if(ent->d_type == DT_REG) {
                /* 
                 * Add an additional byte if the trailing '/' 
                 * is missing in 'd->path'
                 */
                len = sizeof("file://") + len_path + strlen(ent->d_name);
                
                uri = malloc(len);
                if(!uri) {
                    climpd_log_w(tag, "ignoring file %s - %s\n", ent->d_name, 
                                 strerr(ENOMEM));
                    continue;
                }
                
                uri = strcpy(uri, "file://");
                uri = strcat(uri, current_dir);
                uri = strcat(uri, ent->d_name);
                
                if(is_playable(md, uri))
                    dprintf(md->fd, "%s\n", uri + sizeof("file://") - 1);
                
                free(uri);
            }
        }
        
        closedir(dir);
        free(current_dir);
        
        *vector_at(md->dir_vec, i) = NULL;
    }
}