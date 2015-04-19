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
#include <stdio.h>
#include <string.h>
#include <linux/limits.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/error.h>
#include <libvci/filesystem.h>

#define URI_FILE_SCHEME "file://"
#define URI_MAX (PATH_MAX + sizeof(URI_FILE_SCHEME))


static GstDiscoverer *_gst_discoverer;
static char _uri[URI_MAX];
static char _rpath[PATH_MAX];


static char *set_uri(const char *__restrict path, 
                     size_t len, 
                     const char *__restrict name)
{
    if (sizeof(URI_FILE_SCHEME) + len + 1 + strlen(name) >= URI_MAX)
        return NULL;
    
    strcpy(_uri, URI_FILE_SCHEME);
    strcat(_uri, path);
    strcat(_uri, "/");
    strcat(_uri, name);
    
    return _uri;
}

static bool uri_is_playable(const char *__restrict uri)
{
    GstDiscovererResult result;
    GstDiscovererInfo *info;
    GstDiscovererStreamInfo *s_info, *s_info_next;
    GError *err;
    
    info = gst_discoverer_discover_uri(_gst_discoverer, uri, &err);
    
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

int recursive_scan(const char *__restrict path)
{
    DIR *dir;
    struct dirent buf, *ent;
    size_t len;
    int err;
    
    len = strlen(path);
    
    if (len >= PATH_MAX) {
        fprintf(stderr, "path \"%s\" is too long\n", path);
        err = -ENAMETOOLONG;
        return err;
    }
    
    if (!path_is_absolute(path)) {
        if (!realpath(path, _rpath)) {
            err = -errno;
            fprintf(stderr, "unable to retrieve realpath of \"%s\" - %s\n",
                    path, strerr(-err));
            return err;
        }
        
        return recursive_scan(_rpath);
    }
    
    dir = opendir(path);
    if (!dir) {
        err = -errno;
        fprintf(stderr, "failed to open dir \"%s\" - %s\n", path, errstr);
        return err;
    }
    
    if (_rpath != path)
        strcpy(_rpath, path);
    
    while (1) {
        err = readdir_r(dir, &buf, &ent);
        if (err) {
            fprintf(stderr, "reading dir \"%s\" failed - %s\n", path, 
                    strerr(err));
            break;
        }
        
        if (!ent)
            break;
        
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        
        switch (ent->d_type) {
        case DT_REG:
            if (!set_uri(_rpath, len, ent->d_name)) {
                fprintf(stderr, "uri for file \"%s\" in subdir \"%s\""
                        " is too long - skipping file\n", ent->d_name, _rpath);
                continue;
            }
            
            if (!uri_is_playable(_uri))
                continue;
            
            fprintf(stdout, "%s\n", _uri);
            
            break;
        case DT_DIR:
            if (len + 1 + 1 + strlen(ent->d_name) > PATH_MAX) {
                fprintf(stderr, "**ERROR: path for \"%s\" too long in subfolder %s\n", 
                        ent->d_name, _rpath);
                continue;
            }
            
            strcat(_rpath, "/");
            strcat(_rpath, ent->d_name);
            
            err = recursive_scan(_rpath);
            
            _rpath[len] = '\0';
            
            if (err < 0) {
                fprintf(stderr, "failed to scan subdir \"%s\" - %s\n", _rpath, 
                        strerr(-err));
                goto cleanup1;
            }
            break;
        default:
            break;
        }
    }

cleanup1:
    closedir(dir);
    
    return 0;
}

int main(int argc, char *argv[])
{
    GError *error;
    int err;
    
    gst_init(NULL, NULL);
    
    _gst_discoverer = gst_discoverer_new(5 * GST_SECOND, &error);
    if (!_gst_discoverer) {
        fprintf(stderr, "failed to initialize discoverer");
        
        if (error) {
            fprintf(stderr, " - %s", error->message);
            g_error_free(error);
        }
        
        fprintf(stderr, "\n");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 1; i < argc; ++i) {
        err = recursive_scan(argv[i]);
        if (err < 0)
            fprintf(stderr, "failed to scan \"%s\" - %s\n", argv[i], strerr(-err));
    }
    
    gst_object_unref(_gst_discoverer);
    
    gst_deinit();
    
    return EXIT_SUCCESS;
}