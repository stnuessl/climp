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

static const char *tag = "playlist-manager";
static struct map playlist_map;

int playlist_manager_init(void)
{
    struct playlist *pl;
    int err;
    
    err = map_init(&playlist_map, 0, &compare_string, &hash_string);
    if(err < 0)
        goto out;
    
    map_set_data_delete(&playlist_map, (void(*)(void *)) &playlist_delete);
    
    pl = playlist_new(CLIMPD_PLAYER_DEFAULT_PLAYLIST);
    if(!pl) {
        err = -errno;
        goto cleanup1;
    }
    
    err = playlist_manager_insert(pl);
    if(err < 0)
        goto cleanup2;
    
    return 0;

cleanup2:
    playlist_delete(pl);
cleanup1:
    map_destroy(&playlist_map);
out:
    return err;
}

void playlist_manager_destroy(void)
{
    map_destroy(&playlist_map);
}

int playlist_manager_load_from_file(const char *__restrict path)
{
    FILE *f;
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
        
        line[n - 1] = '\0';
        
        if(line[0] != '/') {
            climpd_log_w(tag, "%s is no absolute path\n", line);
            continue;
        }
        
        pl = playlist_new_file(NULL, line);
        if(!pl) {
            msg = strerr(errno);
            climpd_log_w(tag, "playlist_new_file(): %s: %s\n", line, msg);
            continue;
        }
        
        err = playlist_manager_insert(pl);
        if(err < 0) {
            climpd_log_w(tag, "failed to add %s: %s\n", line, strerr(-err));
            continue;
        }
        
        climpd_log_i(tag, "added %s as %s\n", line, playlist_name(pl));
    }
    
    free(line);
    
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

int playlist_manager_insert(struct playlist *__restrict pl)
{
    return map_insert(&playlist_map, playlist_name(pl), pl);
}

struct playlist *playlist_manager_retrieve(const char *__restrict name)
{
    return map_retrieve(&playlist_map, name);
}

struct playlist *playlist_manager_take(const char *__restrict name)
{
    return map_take(&playlist_map, name);
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