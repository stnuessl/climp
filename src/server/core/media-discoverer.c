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
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <linux/limits.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/filesystem.h>
#include <libvci/error.h>

#include <core/media-discoverer.h>
#include <util/climpd-log.h>

#define URI_PREFIX "file://"
#define URI_MAX (PATH_MAX + sizeof(URI_PREFIX))

static const char *tag = "media-discoverer";

static inline const char *uri_get_path(const char *__restrict uri)
{
    return uri + sizeof(URI_PREFIX) - 1;
}

static const char *media_discoverer_set_uri(struct media_discoverer *__restrict md, 
                             const char *__restrict path,
                             size_t len,
                             const char *__restrict name)
{
    if (sizeof(URI_PREFIX) + len + 1 + strlen(name) >= URI_MAX)
        return NULL;
    
    strcpy(md->uri, URI_PREFIX);
    strcat(md->uri, path);
    strcat(md->uri, "/");
    strcat(md->uri, name);
    
    return md->uri;
}

static void report_uri_too_long(const char *__restrict path, 
                                const char *__restrict name)
{
    climpd_log_w(tag, "URI to file \"%s\" in directory \"%s\" is "
                 "too long -> skipping file \n", name, path);
}

static void report_insertion_fail(int err, const char *__restrict path)
{
    climpd_log_e(tag, "failed to create media object for \"%s\" - %s\n", 
                 path, strerr(-err));
}

static bool 
media_discoverer_uri_is_playable(struct media_discoverer *__restrict md)
{
    GstDiscovererResult result;
    GstDiscovererInfo *info;
    GstDiscovererStreamInfo *s_info, *s_info_next;
    GError *err;
    
    info = gst_discoverer_discover_uri(md->discoverer, md->uri, &err);
    
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

int media_discoverer_init(struct media_discoverer *__restrict md)
{
    GError *error;
    int err;
    
    md->discoverer = gst_discoverer_new(5 * GST_SECOND, &error);
    if(!md->discoverer) {
        err = -error->code;
        climpd_log_e(tag, "creating discoverer failed - %s\n", error->message);
        g_error_free(error);
        return err;
    }
    
    md->rpath = malloc(PATH_MAX * sizeof(*md->rpath));
    if (!md->rpath) {
        err = -errno;
        goto cleanup1;
    }
    
    md->uri = malloc(URI_MAX * sizeof(*md->uri));
    if (!md->uri) {
        err = -errno;
        goto cleanup2;
    }
    
    climpd_log_i(tag, "initialized\n");

    return 0;

cleanup2:
    free(md->rpath);
cleanup1:
    gst_object_unref(md->discoverer);
    return err;
}

void media_discoverer_destroy(struct media_discoverer *__restrict md)
{
    free(md->uri);
    free(md->rpath);
    gst_object_unref(md->discoverer);
}


int media_discoverer_scan_dir(struct media_discoverer *__restrict md, 
                              const char *__restrict path,
                              struct media_list *__restrict ml)
{
    DIR *dir;
    struct dirent buf, *ent;
    unsigned int old_size;
    size_t len;
    int err;
    
    if (!path_is_absolute(path)) {
        if (!realpath(path, md->rpath))
            return -errno;
        
        return media_discoverer_scan_dir(md, md->rpath, ml);
    }

    dir = opendir(path);
    if (!dir)
        return -errno;
    
    len = strlen(path);
    old_size = media_list_size(ml);
    
    while (1) {
        err = readdir_r(dir, &buf, &ent);
        if (err) {
            climpd_log_e(tag, "unable to read dir \"%s\" - %s\n", path, 
                         strerr(err));
            goto cleanup1;
        }
        
        if (!ent)
            break;
        
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        
        if (ent->d_type != DT_REG)
            continue;
        
        if (!media_discoverer_set_uri(md, path, len, ent->d_name)) {
            report_uri_too_long(path, ent->d_name);
            continue;
        }

        if (!media_discoverer_uri_is_playable(md))
            continue;
        
        err = media_list_emplace_back(ml, uri_get_path(md->uri));
        if (err < 0) {
            report_insertion_fail(err, uri_get_path(md->uri));
            goto cleanup1;
        }
    }
    
    closedir(dir);
    
    climpd_log_i(tag, "finished scanning \"%s\"\n", path);
    
    return err;
    
cleanup1:
    while (media_list_size(ml) != old_size)
        media_unref(media_list_take_back(ml));

    closedir(dir);

    return err;
}

int media_discoverer_scan_all(struct media_discoverer *__restrict md, 
                              const char *__restrict path,
                              struct media_list *__restrict ml)
{
    DIR *dir;
    struct dirent buf, *ent;
    size_t len;
    unsigned int old_size;
    int err;
    
    len = strlen(path);
    
    if (len >= PATH_MAX) {
        err = -ENAMETOOLONG;
        climpd_log_e(tag, "unable to scan \"%s\" - %s\n", path, strerr(-err));
        return err;
    }
    
    if (!path_is_absolute(path)) {
        if (!realpath(path, md->rpath))
            return -errno;
        
        return media_discoverer_scan_all(md, md->rpath, ml);
    }
    
    dir = opendir(path);
    if (!dir) {
        err = -errno;
        climpd_log_e(tag, "failed to open dir \"%s\" - %s\n", path, errstr);
        return err;
    }
    
    strcpy(md->rpath, path);

    old_size = media_list_size(ml);
    
    while (1) {
        err = readdir_r(dir, &buf, &ent);
        if (err) {
            climpd_log_e(tag, "unable to read dir \"%s\" - %s\n", path, 
                         strerr(err));
            goto cleanup1;
        }
        
        if (!ent)
            break;
        
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        
        switch (ent->d_type) {
        case DT_REG:
            if (!media_discoverer_set_uri(md,  path, len, ent->d_name)) {
                report_uri_too_long(path, ent->d_name);
                continue;
            }
            
            if (!media_discoverer_uri_is_playable(md))
                continue;
            
            err = media_list_emplace_back(ml, uri_get_path(md->uri));
            if (err < 0) {
                report_insertion_fail(err, uri_get_path(md->uri));
                goto cleanup1;
            }
            
            break;
        case DT_DIR:
            if (len + 1 + 1 + strlen(ent->d_name) > PATH_MAX) {
                climpd_log_w(tag, "path to subdirectory \"%s\" in directory "
                             "\"%s\" too long -> skipping subdirectory\n", 
                             ent->d_name, path);
                continue;
            }
            
            strcat(md->rpath, "/");
            strcat(md->rpath, ent->d_name);
            
            err = media_discoverer_scan_all(md, md->rpath, ml);
            
            md->rpath[len] = '\0';
            
            if (err < 0) {
                climpd_log_e(tag, "failed to scan subdirectory \"%s\" at"
                             "\"%s\" - %s\n", ent->d_name, path, strerr(-err));
                goto cleanup1;
            }
            break;
        default:
            break;
        }
    }
    
    closedir(dir);
    
    climpd_log_i(tag, "finished scanning \"%s\"\n", path);
    
    return 0;
    
cleanup1:
    while(media_list_size(ml) != old_size)
        media_unref(media_list_take_back(ml));
    
    closedir(dir);
    
    return err;
}

bool media_discoverer_file_is_playable(struct media_discoverer *__restrict md,
                                       const char *__restrict path)
{
    if (!path_is_absolute(path)) {
        if (!realpath(path, md->rpath))
            return false;
        
        return media_discoverer_file_is_playable(md, md->rpath);
    }
    
    if (strlen(path) + sizeof(URI_PREFIX) >= URI_MAX)
        return false;
    
    strcpy(md->uri, URI_PREFIX);
    strcat(md->uri, path);
    
    return media_discoverer_uri_is_playable(md);
}