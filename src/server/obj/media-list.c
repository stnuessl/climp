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
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <libvci/filesystem.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <obj/media-list.h>
#include <obj/uri.h>

static const char *tag = "media-list";


int media_list_init(struct media_list *__restrict ml)
{
    int err;

    err = vector_init(&ml->media_vec, 32);
    if (err < 0)
        return err;
    
    vector_set_data_delete(&ml->media_vec, (void (*)(void *)) &media_unref);

    return 0;
}

void media_list_destroy(struct media_list *__restrict ml)
{
    vector_destroy(&ml->media_vec);
}

int media_list_insert_back(struct media_list *__restrict ml, struct media *m)
{
    int err;
    m = media_ref(m);
    
    err = vector_insert_back(&ml->media_vec, m);
    if (err < 0)
        media_unref(m);
    
    return err;
}

int media_list_emplace_back(struct media_list *__restrict ml, const char *path)
{
    struct media *m;
    int err;
    
    m = media_new(path);
    if (!m)
        return -errno;
    
    err = vector_insert_back(&ml->media_vec, m);
    if (err < 0)
        media_unref(m);
    
    return err;
}

int media_list_add_from_file(struct media_list *__restrict ml, 
                             const char *__restrict path)
{
    FILE *file;
    char *line;
    const char *ext;
    size_t size;
    ssize_t n;
    unsigned int old_size;
    int err;
    
    ext = strrchr(path, '.');
    if (!ext || (strcmp(ext, ".txt") != 0 && strcmp(ext, ".m3u") != 0)) {
        climpd_log_e(tag, "invalid file extension \"%s\" of playlist \"%s\" - "
                     "must be .txt or .m3u\n", ext, path);
        return -EINVAL;
    }
    
    file = fopen(path, "r");
    if(!file)
        return -errno;
    
    line = NULL;
    size = 0;
    err  = 0;
    
    old_size = vector_size(&ml->media_vec);
    
    while(1) {
        n = getline(&line, &size, file);
        if (n < 0)
            break;
        
        if (n == 0)
            continue;
        
        if (line[0] == '#' || line[0] == '\n')
            continue;
        
        line[n - 1] = '\0';
                
        if (!path_is_absolute(line) && !uri_ok(line)) {
            err = -ENOTSUP;
            climpd_log_e(tag, "\"%s\" - no absolute path or valid uri\n", line);
            goto cleanup1;
        }
        
        err = media_list_emplace_back(ml, line);
        if (err < 0) {
            climpd_log_w(tag, "failed to initialize media file '%s' - %s\n", 
                         line, strerr(-err));
            goto cleanup1;
        }
    }
    
    free(line);
    fclose(file);
    
    if (vector_size(&ml->media_vec) == old_size)
        climpd_log_i(tag, "loaded empty playlist file \"%s\"\n", path);
    
    return 0;
    
cleanup1:
    while (vector_size(&ml->media_vec) != old_size)
        media_unref(vector_take_back(&ml->media_vec));
    
    free(line);
    fclose(file);
    
    return err;
}

struct media *media_list_at(struct media_list *__restrict ml, 
                            unsigned int index)
{
    assert(index < media_list_size(ml) && "INVALID INDEX");
    
    return media_ref(*vector_at(&ml->media_vec, index));
}

struct media *media_list_take(struct media_list *__restrict ml, 
                              unsigned int index)
{
    assert(index < media_list_size(ml) && "INVALID INDEX");
    
    return vector_take_at(&ml->media_vec, index);
}

struct media *media_list_take_back(struct media_list *__restrict ml)
{
    return vector_take_back(&ml->media_vec);
}

unsigned int media_list_size(const struct media_list *__restrict ml)
{
    return vector_size(&ml->media_vec);
}

bool media_list_empty(const struct media_list *__restrict ml)
{
    return vector_empty(&ml->media_vec);
}
