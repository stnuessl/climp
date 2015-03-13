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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libvci/filesystem.h>

#include <core/media_list_loader.h>

int media_list_loader_init(struct media_list_loader *__restrict loader,
                           const char *__restrict path)
{
    struct stat st;
    size_t len;
    int err;
    
    loader->dir_path = strcpy(path);
    if (!loader->dir_path)
        return -errno;
    
    loader->d_path_len = strlen(loader->dir_path);

    err = stat(loader->dir_path, &st);
    if (err < 0) {
        if (errno != ENOENT) {
            err = -errno;
            goto cleanup1;
        }
     
        err = path_create(loader->dir_path, 755);
        if (err < 0)
            goto cleanup1;
    }
    
    if (!S_ISDIR(st.st_mode))
        return -ENOTDIR;
    
    loader->file_path = malloc(PATH_MAX * sizeof(*loader->file_path));
    if (!loader->file_path) {
        err = -errno;
        goto cleanup1;
    }
    
    return 0;
    
cleanup1:
    free(loader->dir_path);
    return err;
}

void media_list_loader_destroy(struct media_list_loader *__restrict loader)
{
    free(loader->file_path);
    free(loader->dir_path);
}

int media_list_loader_load(struct media_list_loader *__restrict loader,
                           const char *__restrict path,
                           struct media_list *__restrict ml)
{
    int err;
    
    if (path_is_reg(path)) {
        err = media_list_add_from_file(ml, path);
        if (err == 0)
            return 0;
    }
    
    if (loader->d_path_len + strlen(path) + 2 >= PATH_MAX)
        return -ENAMETOOLONG;
    
    strcpy(loader->file_path, loader->dir_path);
    
    if (loader->file_path[loader->d_path_len] != '/')
        strcat(loader->file_path, "/");
    
    strcat(loader->file_path, path);
    
    return media_list_add_from_file(ml, loader->file_path);
}