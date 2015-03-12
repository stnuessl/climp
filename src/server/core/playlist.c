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
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <libvci/vector.h>
#include <libvci/macro.h>
#include <libvci/random.h>
#include <libvci/error.h>
#include <libvci/filesystem.h>

#include <util/climpd-log.h>
#include <core/playlist.h>
#include <core/media.h>

static const char *tag = "playlist";



int playlist_init(struct playlist *__restrict pl, bool repeat, bool shuffle)
{
    void (*del)(void *);
    int (*cmp)(const void *, const void *);
    int err;

    err = vector_init(&pl->vec_media, 32);
    if(err < 0)
        return err;
    
    del = (void (*)(void *)) &media_unref;
    cmp = (int (*)(const void *, const void *)) &media_compare;
    
    vector_set_data_delete(&pl->vec_media, del);
    vector_set_data_compare(&pl->vec_media, cmp);
    
    err = kfy_init(&pl->kfy, 0);
    if (err < 0) {
        vector_destroy(&pl->vec_media);
        return err;
    }
    
    pl->index = 0;
    
    pl->repeat  = repeat;
    pl->shuffle = shuffle;
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;
}

void playlist_destroy(struct playlist *__restrict pl)
{
    kfy_destroy(&pl->kfy);
    vector_destroy(&pl->vec_media);
}

void playlist_clear(struct playlist *__restrict pl)
{
    pl->index = 0;
    kfy_destroy(&pl->kfy);
    kfy_init(&pl->kfy, 0);
    vector_clear(&pl->vec_media);
}

int playlist_insert_back(struct playlist *__restrict pl, struct media *m)
{
    int err;
    
    m = media_ref(m);
    
    err = vector_insert_back(&pl->vec_media, m);
    if (err < 0)
        goto cleanup1;
    
    err = kfy_add(&pl->kfy, 1);
    if (err < 0)
        goto cleanup2;
    
    assert(kfy_size(&pl->kfy) == playlist_size(pl) && "INVALID SIZE");
    
    return err;

cleanup2:
    vector_take_back(&pl->vec_media);
cleanup1:
    media_unref(m);
    return err;
}

int playlist_emplace_back(struct playlist *__restrict pl, const char *path)
{
    struct media *m;
    int err;
    
    m = media_new(path);
    if (!m)
        return -errno;
    
    err = playlist_insert_back(pl, m);
    if (err < 0)
        media_unref(m);
    
    return err;
}

int playlist_add_media_list(struct playlist *__restrict pl, 
                            struct media_list *__restrict ml)
{
    unsigned int old_size, size;
    int err;
    
    if (media_list_empty(ml))
        return 0;
    
    old_size = vector_size(&pl->vec_media);
    size     = media_list_size(ml);

    for (unsigned int i = 0; i < size; ++i) {
        err = vector_insert_back(&pl->vec_media, media_list_at(ml, i));
        if (err < 0)
            goto cleanup1;
    }
    
    err = kfy_add(&pl->kfy, size);
    if (err < 0)
        goto cleanup1;
    
    assert(kfy_size(&pl->kfy) == playlist_size(pl) && "INVALID SIZE");
    
    return 0;
    
cleanup1:
    while (vector_size(&pl->vec_media) != old_size)
        media_unref(vector_take_back(&pl->vec_media));
    
    return err;
}

void playlist_remove_media_list(struct playlist *__restrict pl, 
                                struct media_list *__restrict ml)
{
    unsigned int size, cnt;
    
    cnt  = 0;
    size = media_list_size(ml);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = media_list_at(ml, i);
        struct media *n = vector_take(&pl->vec_media, m);
        
        if (n) {
            cnt++;
            media_unref(n);
        }
        
        media_unref(m);
    }
    
    kfy_remove(&pl->kfy, cnt);
}

struct media *playlist_at(struct playlist *__restrict pl, int index)
{
    assert((unsigned int) abs(index) < playlist_size(pl) && "INVALID INDEX");
    
    if (index < 0)
        index = playlist_size(pl) + index;

    return media_ref(*vector_at(&pl->vec_media, (unsigned int) index));
}

struct media *playlist_take(struct playlist *__restrict pl, int index)
{
    assert((unsigned int) abs(index) < playlist_size(pl) && "INVALID INDEX");
    
    if (index < 0)
        index = playlist_size(pl) + index;
    
    kfy_remove(&pl->kfy, 1);
    
    return vector_take_at(&pl->vec_media,  (unsigned int) index);
}

void playlist_update_index(struct playlist *__restrict pl, int index)
{
    if (index < 0)
        index = playlist_size(pl) + index;
    
    /* 
     * Only relevant if not in random mode, so playlist_next() will get 
     * the right next track. Note how playlist_next() will handle 
     * an invalid index.
     */
    pl->index = (unsigned int) index + 1;
}

unsigned int playlist_index_of(const struct playlist *__restrict pl,
                               const struct media *__restrict m)
{
    return vector_index_of(&pl->vec_media, m);
}

unsigned int playlist_index_of_path(struct playlist *__restrict pl,
                                    const char *__restrict path)
{    
    unsigned int size = vector_size(&pl->vec_media);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = *vector_at(&pl->vec_media, i);
        
        if (media_path_compare(m, path) == 0)
            return i;
    }
    
    return (unsigned int) -1;
}

unsigned int playlist_size(const struct playlist *__restrict pl)
{
    return vector_size(&pl->vec_media);
}

bool playlist_empty(const struct playlist *__restrict pl)
{
    return vector_empty(&pl->vec_media);
}

struct media *playlist_next(struct playlist *__restrict pl)
{
    if (vector_empty(&pl->vec_media))
        return NULL;
    
    if (pl->shuffle) {
        if (kfy_cycle_done(&pl->kfy) && !pl->repeat) {
            kfy_reset(&pl->kfy);
            return NULL;
        }
        
        pl->index = kfy_shuffle(&pl->kfy);
        
        assert(pl->index < playlist_size(pl) && "INVALID INDEX");
        
        return media_ref(*vector_at(&pl->vec_media, pl->index));
    }
   
    if (pl->index < vector_size(&pl->vec_media))
        return media_ref(*vector_at(&pl->vec_media, pl->index++));
    
    pl->index = 0;
    
    if (pl->repeat)
        return media_ref(*vector_at(&pl->vec_media, pl->index++));
    
    return NULL;
}

bool playlist_toggle_shuffle(struct playlist *__restrict pl)
{
    pl->shuffle = !pl->shuffle;
    
    return pl->shuffle;
}

bool playlist_toggle_repeat(struct playlist *__restrict pl)
{
    pl->repeat = !pl->repeat;
    
    return pl->repeat;
}

void playlist_set_shuffle(struct playlist *__restrict pl, bool shuffle)
{
    pl->shuffle = shuffle;
}

void playlist_set_repeat(struct playlist *__restrict pl, bool repeat)
{
    pl->repeat = repeat;
}

bool playlist_shuffle(const struct playlist *__restrict pl)
{
    return pl->shuffle;
}

bool playlist_repeat(const struct playlist *__restrict pl)
{
    return pl->repeat;
}

