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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <libvci/map.h>
#include <libvci/hash.h>
#include <libvci/compare.h>
#include <libvci/error.h>

#include "../../shared/constants.h"
#include "../../shared/util.h"

#include "playlist.h"
#include "climpd-log.h"
#include "playlist-manager.h"
#include "media-manager.h"

static const char *tag = "playlist-manager";
static struct map playlist_map;

static int load_files_from_file(struct playlist *__restrict pl, 
                                const char *path)
{
    struct media *m;
    struct stat st;
    FILE *file;
    char *line;
    size_t size;
    ssize_t n;
    int err;
    
    file = fopen(path, "r");
    if(!file) {
        err = -errno;
        climpd_log_e(tag, "fopen() - %s\n", errstr);
        return err;
    }
    
    line  = NULL;
    size = 0;
    
    while(1) {
        n = getline(&line, &size, file);
        if(n < 0)
            break;
        
        if(n == 0)
            continue;
        
        if(line[0] == '#' || line[0] == ';')
            continue;
        
        line[n - 1] = '\0';
        
        if(line[0] != '/') {
            climpd_log_w(tag, "skipping %s - no absolute path\n", line);
            continue;
        }
        
        err = stat(line, &st);
        if(err < 0) {
            climpd_log_w(tag, "stat(): %s - %s\n", line, errstr);
            continue;
        }
        
        if(!S_ISREG(st.st_mode)) {
            climpd_log_w(tag, "%s is no regular file\n", line);
            continue;
        }
        
        m = media_manager_retrieve(line);
        if(!m) {
            climpd_log_w(tag, "skipping %s - failed to retrieve media\n", line);
            continue;
        }
        
        err = playlist_insert_back(pl, m);
        if(err < 0) {
            climpd_log_w(tag, "failed to add %s to playlist\n", line);
            media_manager_delete_media(path);
            continue;
        }
    }
    
    free(line);
    fclose(file);
    
    if(playlist_empty(pl)) {
        climpd_log_e(tag, "loaded empty playlist\n");
        return -ENOENT;
    }
    
    return 0;
}

static struct playlist *
new_playlist_from_file(const char *__restrict path)
{
    struct playlist *pl;
    const char *name;
    int err;
    
    name = (path[0] == '/') ? strrchr(path, '/') + 1 : path;
    
    pl = playlist_new(name);
    if(!pl) {
        err = -errno;
        goto out;
    }
    
    err = load_files_from_file(pl, path);
    if(err < 0) {
        playlist_delete(pl);
        goto out;
    }
    
    climpd_log_i(tag, "created playlist '%s' from '%s'\n", name, path);
    
    return pl;

out:
    climpd_log_e(tag, "failed to create playlist '%s' from '%s'", name, path);
    climpd_log_append(" - %s\n", strerr(-err));
    errno = -err;
    return 0;
}

static int playlist_manager_insert(struct playlist *__restrict pl)
{
    return map_insert(&playlist_map, playlist_name(pl), pl);
}

static struct playlist *playlist_manager_take(const char *__restrict name)
{
    name = (name[0] == '/') ? strrchr(name, '/') + 1 : name;
    
    return map_take(&playlist_map, name);
}

int playlist_manager_init(void)
{
    struct playlist *pl;
    int err;
    
    err = map_init(&playlist_map, 0, &compare_string, &hash_string);
    if(err < 0)
        return err;
    
    map_set_data_delete(&playlist_map, (void(*)(void *)) &playlist_delete);
    
    pl = playlist_new(CLIMPD_DEFAULT_PLAYLIST);
    if(!pl) {
        err = -errno;
        goto cleanup1;
    }
    
    err = playlist_manager_insert(pl);
    if(err < 0)
        goto cleanup2;
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;
    
cleanup2:
    map_destroy(&playlist_map);
cleanup1:
    playlist_delete(pl);
    
    return err;
}

void playlist_manager_destroy(void)
{
    map_destroy(&playlist_map);
    climpd_log_i(tag, "destroyed\n");
}

int playlist_manager_load_from_file(const char *__restrict path)
{
    FILE *f;
    struct stat st;
    struct playlist *pl;
    const char *msg;
    char *line;
    size_t size;
    ssize_t n;
    int err;
    
    f = fopen(path, "r");
    if(!f)
        return -errno;
    
    line = NULL;
    size = 0;
    
    while(1) {
        n = getline(&line, &size, f);
        if(n < 0)
            break;
        
        if(n == 0)
            continue;
        
        if(line[0] == '#' || line[0] == ';')
            continue;
        
        line[n - 1] = '\0';
        
        if(line[0] != '/') {
            climpd_log_w(tag, "%s is no absolute path\n", line);
            continue;
        }
        
        err = stat(line, &st);
        if(err < 0) {
            climpd_log_w(tag, "stat(): %s - %s\n", line, errstr);
            continue;
        }
        
        if(!S_ISREG(st.st_mode)) {
            climpd_log_w(tag, "%s is no regular file\n", line);
            continue;
        }
        
        pl = new_playlist_from_file(line);
        if(!pl) {
            msg = strerr(errno);
            climpd_log_w(tag, "%s: %s\n", line, msg);
            continue;
        }
        
        err = playlist_manager_insert(pl);
        if(err < 0) {
            climpd_log_w(tag, "failed to add %s: %s\n", line, strerr(-err));
            continue;
        }
    }
    
    free(line);
    fclose(f);
    
    return 0;
}

int playlist_manager_save_to_file(const char *__restrict path)
{
    int fd, err;
    
    err = create_leading_directories(path, DEFAULT_DIR_MODE);
    if(err < 0)
        return err;
    
    fd = open(path, O_RDONLY | O_CREAT | O_TRUNC, DEFAULT_FILE_MODE);
    if(fd < 0)
        return -errno;
    
    playlist_manager_print(fd);
    
    close(fd);
    
    return 0;
}

struct playlist *playlist_manager_retrieve(const char *name)
{
    static const char *err_msg = "cannot create playlist '%s' - 's'\n";
    struct playlist *pl;
    const char *key;
    int err;
    
    key = (name[0] == '/') ? strrchr(name, '/') + 1 : name;
    
    pl = map_retrieve(&playlist_map, key);
    if(!pl) {
        pl = new_playlist_from_file(name);
        if(!pl) {
            climpd_log_e(tag, err_msg, name, errstr);
            return NULL;
        }
        
        err = map_insert(&playlist_map, key, pl);
        if(err < 0) {
            climpd_log_e(tag, err_msg, name, strerr(-err));
            playlist_delete(pl);
            return NULL;
        }
    }
    
    return pl;
}

void playlist_manager_delete_playlist(const char *__restrict name)
{
    struct playlist *pl;

    pl = playlist_manager_take(name);
    if(!pl)
        return;
    
    playlist_delete(pl);
}

void playlist_manager_print(int fd)
{
    struct entry *e;
    
    map_for_each(&playlist_map, e)
        dprintf(fd, " %s\n", (const char *) entry_key(e));
}