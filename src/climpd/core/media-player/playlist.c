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
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <libvci/vector.h>
#include <libvci/macro.h>
#include <libvci/random.h>
#include <libvci/error.h>
#include <libvci/filesystem.h>

#include <core/media-player/playlist.h>
#include <core/media-player/kfy.h>
#include <core/climpd-log.h>
#include <core/util.h>

static const char *tag = "playlist";

static struct vector _vec_media;
static unsigned int _index;
static bool _repeat;
static bool _shuffle;

void playlist_init(bool repeat, bool shuffle)
{
    void (*del)(void *);
    int (*cmp)(const void *, const void *);
    int err;


    err = vector_init(&_vec_media, 32);
    if(err < 0) {
        climpd_log_e(tag, "failed to initialize media playlist - %s\n", 
                     strerr(-err));
        goto fail;
    }
    
    del = (void (*)(void *)) &media_unref;
    cmp = (int (*)(const void *, const void *)) &media_compare;
    
    vector_set_data_delete(&_vec_media, del);
    vector_set_data_compare(&_vec_media, cmp);
    
    kfy_init(0);
    
    _index   = (unsigned int) -1;
    _repeat  = repeat;
    _shuffle = shuffle;
    
    return;
    
fail:
    die_failed_init(tag);
}

void playlist_destroy(void)
{
    kfy_destroy();
    vector_destroy(_vec_media);
}

void playlist_clear(void)
{
    _index = (unsigned int) -1;
    kfy_destroy();
    kfy_init(0);
    vector_clear(&_vec_media);
}

int playlist_insert_back(struct media *m)
{
    int err;
    
    m = media_ref(m);
    
    err = vector_insert_back(&_vec_media, m);
    if (err < 0)
        goto cleanup1;
    
    err = kfy_add(1);
    if (err < 0)
        goto cleanup2;
    
    assert(kfy_size(&pl->kfy) == playlist_size(pl) && "INVALID SIZE");
    
    return err;

cleanup2:
    vector_take_back(&_vec_media);
cleanup1:
    media_unref(m);
    return err;
}

int playlist_emplace_back(const char *path)
{
    struct media *m;
    int err;
    
    m = media_new(path);
    if (!m)
        return -errno;
    
    err = playlist_insert_back(m);
    if (err < 0)
        media_unref(m);
    
    return err;
}

int playlist_set_media_list(struct media_list *__restrict ml)
{
    unsigned int size, ml_size;
    int err;

    size    = vector_size(&_vec_media);
    ml_size = media_list_size(ml);

    if (size >= ml_size) {
        while (vector_size(&_vec_media) > ml_size)
            media_unref(vector_take_back(&_vec_media));
        
        for (unsigned int i = 0; i < ml_size; ++i) {
            struct media **m = (struct media **) vector_at(&_vec_media, i);
            
            media_unref(*m);
            *m = media_list_at(ml, i);
        }
        
        kfy_remove(size - ml_size);
        
        return 0;
    }
    
    err = vector_set_capacity(&_vec_media, ml_size);
    if (err < 0)
        return err;
    
    err = kfy_add(ml_size - size);
    if (err < 0)
        return err;
    
    vector_clear(&_vec_media);
    
    for (unsigned int i = 0; i < ml_size; ++i)
        vector_insert_back(&_vec_media, media_list_at(ml, i));
    
    return 0;
}

int playlist_add_media_list(struct media_list *__restrict ml)
{
    unsigned int old_size, size;
    int err;
    
    if (media_list_empty(ml))
        return 0;
    
    old_size = vector_size(&_vec_media);
    size     = media_list_size(ml);

    for (unsigned int i = 0; i < size; ++i) {
        err = vector_insert_back(&_vec_media, media_list_at(ml, i));
        if (err < 0)
            goto cleanup1;
    }
    
    err = kfy_add(size);
    if (err < 0)
        goto cleanup1;
    
    assert(kfy_size(&pl->kfy) == playlist_size() && "INVALID SIZE");
    
    return 0;
    
cleanup1:
    while (vector_size(&_vec_media) > old_size)
        media_unref(vector_take_back(&_vec_media));
    
    return err;
}

void playlist_remove_media_list(struct media_list *__restrict ml)
{
    unsigned int size, cnt;
    
    cnt  = 0;
    size = media_list_size(ml);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = media_list_at(ml, i);
        struct media *n = vector_take(&_vec_media, m);
        
        if (n) {
            cnt++;
            media_unref(n);
        }
        
        media_unref(m);
    }
    
    kfy_remove(cnt);
}

struct media *playlist_at(int index)
{
    assert((unsigned int) abs(index) < playlist_size(pl) && "INVALID INDEX");
    
    if (index < 0)
        index = playlist_size() + index;

    return media_ref(*vector_at(&_vec_media, (unsigned int) index));
}

struct media *playlist_take(int index)
{
    assert((unsigned int) abs(index) < playlist_size(pl) && "INVALID INDEX");
    
    if (index < 0)
        index = playlist_size() + index;
    
    kfy_remove(1);
    
    return vector_take_at(_vec_media,  (unsigned int) index);
}

void playlist_update_index(int index)
{
    if (index < 0)
        index = playlist_size() + index;
    
    /* 
     * Only relevant if not in random mode, so playlist_next() will get 
     * the right next track. Note how playlist_next() will handle 
     * an invalid index.
     */
    _index = (unsigned int) index;
}

unsigned int playlist_index(void)
{
    return _index;
}

unsigned int playlist_index_of(const struct media *__restrict m)
{
    return vector_index_of(_vec_media, m);
}

unsigned int playlist_index_of_path(const char *__restrict path)
{    
    unsigned int size = vector_size(&_vec_media);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = *vector_at(&_vec_media, i);
        
        if (media_hierarchical_compare(m, path) == 0)
            return i;
    }
    
    return (unsigned int) -1;
}

unsigned int playlist_size(void)
{
    return vector_size(&_vec_media);
}

bool playlist_empty(void)
{
    return vector_empty(&_vec_media);
}

void playlist_sort(void)
{
    vector_sort(&_vec_media);
    kfy_reset();
    
    _index = 0;
}

struct media *playlist_next(void)
{
    if (vector_empty(&_vec_media))
        return NULL;
    
    if (_shuffle) {
        if (kfy_cycle_done() && !_repeat) {
            kfy_reset();
            return NULL;
        }
        
        _index = kfy_shuffle();
        
        assert(pl->index < playlist_size(pl) && "INVALID INDEX");
        
        return media_ref(*vector_at(&_vec_media, _index));
    }
    
    ++_index;
    
    if (_index >= vector_size(&_vec_media)) {
        if (!_repeat) {
            _index = (unsigned int) -1;
            return NULL;
        }
        
        _index = 0;
    }
    
    return media_ref(*vector_at(&_vec_media, _index));
}

bool playlist_toggle_shuffle(void)
{
    _shuffle = !_shuffle;
    
    return _shuffle;
}

bool playlist_toggle_repeat(void)
{
    _repeat = !_repeat;
    
    return _repeat;
}

void playlist_set_shuffle(bool shuffle)
{
    _shuffle = shuffle;
}

void playlist_set_repeat(bool repeat)
{
    _repeat = repeat;
}

bool playlist_shuffle(void)
{
    return _shuffle;
}

bool playlist_repeat(void)
{
    return _repeat;
}

static void load_playlist_from_disk(void)
{
    const char *path = climpd_paths_last_playlist();
    struct media_list ml;
    int err;
    
    err = media_list_init(&ml);
    if (err < 0) {
        climpd_log_w(tag, "failed to initialize last played playlist\n");
        return;
    }
    
    err = media_list_add_from_path(&ml, path);
    if (err < 0 && err != -ENOENT) {
        climpd_log_w(tag, "failed to load last playlist elements from \"%s\" - "
        "%s\n", path, strerr(-err));
        goto cleanup;
    }
    
    err = playlist_add_media_list(&ml);
    if (err < 0) {
        climpd_log_w(tag, "failed to add last playlist elements - %s\n", 
                     strerr(-err));
        goto cleanup;
    }
    
cleanup:
    media_list_destroy(&ml);
}

void playlist_save(const char *__restrict path)
{
    unsigned int size;
    int fd;
    
    fd = open(path, O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        climpd_log_e(tag, "failed to save playlist to \"%s\" - %s\n", path, 
                     errstr);
        return;
    }
    
    size = vector_size(_vec_media);
    
    for (unsigned int i = 0; i < size; ++i)
        dprintf(fd, "%s\n", media_uri(*vector_at(_vec_media, i)));
    
    close(fd);
    
    climpd_log_i(tag, "saved playlist to '%s'\n", path);
}

void playlist_load(const char *__restrict path)
{
    struct media_list ml;
    int err;
    
    err = media_list_init(&ml);
    if (err < 0) {
        climpd_log_w(tag, "failed to load playlist - %s\n", strerr(-err));
        return;
    }
    
    err = media_list_add_from_path(&ml, path);
    if (err < 0 && err != -ENOENT) {
        climpd_log_w(tag, "failed to load playlist elements from '%s' - "
                     "%s\n", path, strerr(-err));
        goto cleanup;
    }
    
    err = playlist_add_media_list(&ml);
    if (err < 0) {
        climpd_log_w(tag, "failed to add last playlist elements - %s\n", 
                     strerr(-err));
        goto cleanup;
    }
    
    climpd_log_i(tag, "loaded playlist '%s'\n", path);
    
cleanup:
    media_list_destroy(&ml);
}

