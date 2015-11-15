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
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#include <libvci/filesystem.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/playlist/playlist.h>

#include <media/uri.h>


static const char *tag = "playlist";


static unsigned int ensure_positiv_index(const struct playlist *__restrict pl, 
                                         int index)
{
    if (index < 0)
        return vector_size(&pl->vec_media) + index;
    else
        return index;
}

int playlist_init(struct playlist *__restrict pl)
{
    void (*del)(void *);
    int (*cmp)(const void *, const void *);
    int err;
    
    memset(pl, 0, sizeof(*pl));
    
    err = vector_init(&pl->vec_media, 64);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize vector - %s\n", strerr(-err));
        return err;
    }
    
    del = (void (*)(void *)) &media_unref;
    cmp = (int (*)(const void *, const void *)) &media_compare;
    
    vector_set_data_delete(&pl->vec_media, del);
    vector_set_data_compare(&pl->vec_media, cmp);
    
    err = tag_reader_init(&pl->tag_reader);
    if (err < 0) {
        err = -ENOTSUP;
        climpd_log_e(tag, "failed to initialize tag-reader\n");
        goto cleanup1;
    }
    
    err = kfy_init(&pl->kfy, 0);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize track shuffler - %s\n", 
                     strerr(-err));
        goto cleanup2;
    }
    
    pl->index = (unsigned int) -1;
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;
    
cleanup2:
    tag_reader_destroy(&pl->tag_reader);
cleanup1:
    vector_destroy(&pl->vec_media);
    
    return err;
}

void playlist_destroy(struct playlist *__restrict pl)
{
    kfy_destroy(&pl->kfy);
    tag_reader_destroy(&pl->tag_reader);
    vector_destroy(&pl->vec_media);
    
    climpd_log_i(tag, "destroyed\n");
}

void playlist_clear(struct playlist *__restrict pl)
{
    unsigned int size = kfy_size(&pl->kfy);
    
    assert(size == vector_size(&pl->vec_media) && "INVALID SIZE");
    
    kfy_remove(&pl->kfy, size);
    vector_clear(&pl->vec_media);
    
    pl->index = (unsigned int) -1;
}

int playlist_add_media(struct playlist *__restrict pl, struct media *m)
{
    int err;
    
    if (!media_is_parsed(m))
        tag_reader_read_async(&pl->tag_reader, m);
    
    media_ref(m);
    
    err = vector_insert_back(&pl->vec_media, m);
    if (err < 0) {
        const char *uri = media_uri(m);
        
        climpd_log_e(tag, "failed to add '%s' - %s\n", uri, strerr(-err));
        
        media_unref(m);
        
        return err;
    }
    
    err = kfy_add(&pl->kfy, 1);
    if (err < 0) {
        climpd_log_e(tag, "failed to adjust track shuffler - %s\n", 
                     strerr(-err));
        
        vector_take_back(&pl->vec_media);
        media_unref(m);
        return err;
    }
    
    assert(kfy_size(&pl->kfy) == vector_size(&pl->vec_media) && "INVALID SIZE");
    
    return 0;
}

int playlist_add(struct playlist *__restrict pl, const char *__restrict path)
{
    struct media *m;
    int err;
    
    m = media_new(path);
    if (!m) {
        err = -errno;
        climpd_log_e(tag, "failed to create media '%s' - %s\n", path, errstr);
        return err;
    }
    
    err = playlist_add_media(pl, m);
    media_unref(m);
    
    return err;
}

int playlist_load(struct playlist *__restrict pl, const char *__restrict path)
{
    FILE *file;
    char *line;
    size_t size;
    ssize_t n;
    unsigned int old_size;
    int err;
    
    file = fopen(path, "r");
    if (!file) {
        err = -errno;
        climpd_log_e(tag, "failed to open/load '%s' - %s\n", path, errstr);
        return err;
    }
    
    line = NULL;
    size = 0;
    
    old_size = playlist_size(pl);
    
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
        
        err = playlist_add(pl, line);
        if (err < 0)
            goto cleanup1;
    }
    
    free(line);
    fclose(file);
    
    climpd_log_i(tag, "loaded '%s'\n", path);
    
    return 0;
    
cleanup1:
    while (playlist_size(pl) != old_size)
        playlist_remove(pl, -1);

    free(line);
    fclose(file);
    
    return err;
}

int playlist_save(struct playlist *__restrict pl, const char *__restrict path)
{
    unsigned int size;
    int fd;
    
    fd = open(path, O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fd = -errno;
        climpd_log_e(tag, "failed to save as '%s' - %s\n", path, errstr);
        return fd;
    }
    
    size = vector_size(&pl->vec_media);
    
    for (unsigned int i = 0; i < size; ++i)
            dprintf(fd, "%s\n", media_uri(*vector_at(&pl->vec_media, i)));
    
    close(fd);
    
    return 0;
}

unsigned int playlist_index_of(struct playlist *__restrict pl, 
                               const char *__restrict path)
{
    unsigned int size = vector_size(&pl->vec_media);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = *vector_at(&pl->vec_media, i);
        
        if (media_path_compare(m, path))
            return i;
    }
    
    return (unsigned int) -1;
}

void playlist_remove(struct playlist *__restrict pl, int index)
{
    unsigned int i = ensure_positiv_index(pl, index);
    
    if (i < playlist_size(pl)) {
        struct media *m = vector_take_at(&pl->vec_media, i);
        
        media_unref(m);
    }
}

unsigned int playlist_index(const struct playlist *__restrict pl)
{
    return pl->index;
}

void playlist_set_index(struct playlist *__restrict pl, int index)
{
    pl->index = ensure_positiv_index(pl, index);
}

struct media *playlist_at(struct playlist *__restrict pl, int index)
{
    struct media *m;
    unsigned int i;
    
    i = ensure_positiv_index(pl, index);
    
    if (i >= vector_size(&pl->vec_media))
        return NULL;
    
    m = *vector_at(&pl->vec_media, i);
        
    return media_ref(m);
}

void playlist_set_shuffle(struct playlist *__restrict pl, bool shuffle)
{
    pl->shuffle = shuffle;
}

bool playlist_shuffle(const struct playlist *__restrict pl)
{
    return pl->shuffle;
}

void playlist_toggle_shuffle(struct playlist *__restrict pl)
{
    pl->shuffle = !pl->shuffle;
}

void playlist_set_repeat(struct playlist *__restrict pl, bool repeat)
{
    pl->repeat = repeat;
}

bool playlist_repeat(const struct playlist *__restrict pl)
{
    return pl->repeat;
}

void playlist_toggle_repeat(struct playlist *__restrict pl)
{
    pl->repeat = !pl->repeat;
}

unsigned int playlist_next(struct playlist *__restrict pl)
{
    if (vector_empty(&pl->vec_media))
        return -1;
    
    if (pl->shuffle) {
        if (kfy_cycle_done(&pl->kfy) && !pl->repeat) {
            kfy_reset(&pl->kfy);
            return (unsigned int) -1;
        }
        
        pl->index = kfy_shuffle(&pl->kfy);
        
        assert(pl->index < playlist_size(pl) && "INVALID INDEX");
        
        return (unsigned int) -1;
    }
    
    ++pl->index;
    
    if (pl->index >= vector_size(&pl->vec_media)) {
        if (!pl->repeat) {
            pl->index = (unsigned int) -1;
            return pl->index;
        }
        
        pl->index = 0;
    }
    
    return pl->index;
}

unsigned int playlist_size(const struct playlist *__restrict pl)
{
    return vector_size(&pl->vec_media);
}

bool playlist_empty(const struct playlist *__restrict pl)
{
    return vector_empty(&pl->vec_media);
}

void playlist_sort(struct playlist *__restrict pl)
{
    vector_sort(&pl->vec_media);
    kfy_reset(&pl->kfy);
    
    pl->index = (unsigned int) -1;
}