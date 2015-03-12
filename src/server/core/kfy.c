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
#include <errno.h>
#include <assert.h>

#include <core/kfy.h>

#define KFY_MIN_CAPACITY 8

static unsigned int next_pow_2(unsigned int val)
{
    unsigned int ret = KFY_MIN_CAPACITY;
    
    while (ret <= val)
        ret <<= 1;
    
    return ret;
}

int kfy_init(struct kfy *__restrict k, unsigned int size)
{
    unsigned int cap;
    int err;
    
    err = random_init(&k->rand);
    if (err < 0)
        return err;
    
    cap = next_pow_2(size);
    
    k->a = malloc(cap * sizeof(*k->a));
    if (!k->a) {
        err = -errno;
        random_destroy(&k->rand);
        return -err;
    }
    
    for (unsigned int i = 0; i < size; ++i)
        k->a[i] = i;
    
    k->end      = size;
    k->size     = size;
    k->capacity = cap;
    
    return 0;
}

void kfy_destroy(struct kfy *__restrict k)
{
    free(k->a);
    random_destroy(&k->rand);
}

void kfy_reset(struct kfy *__restrict k)
{
    if (k->size < k->capacity >> 2 && k->size >= KFY_MIN_CAPACITY) {
        unsigned int new_cap, *a;
        
        new_cap = next_pow_2(k->size);
        
        assert(new_cap < k->capacity && "INVALID CAPACITY");
        
        a = realloc(k->a, new_cap * sizeof(*k->a));
        if (a) {
            k->a        = a;
            k->capacity = new_cap;
        }
    }
    
    for (unsigned int i = 0; i < k->size; ++i)
        k->a[i] = i;
    
    k->end = k->size;
}

unsigned int kfy_shuffle(struct kfy *__restrict k)
{
    unsigned int val, index;
    
    if (k->end == 0)
        kfy_reset(k);
    
    k->end -= 1;
    
    index = random_uint_range(&k->rand, 0, k->end);
    
    val = k->a[index];
    k->a[index] = k->a[k->end];
    
    return val;
}

int kfy_add(struct kfy *__restrict k, unsigned int cnt)
{
    unsigned int new_size, new_cap, new_end, *a;
    
    new_size = k->size + cnt;
    new_end  = k->end + cnt;
    
    if (new_size < k->capacity) {
        for (unsigned int i = k->end; i < new_end; ++i)
            k->a[i] = k->size + i - k->end;

        k->size = new_size;
        k->end  = new_end;
        
        return 0;
    }
    
    new_cap  = next_pow_2(new_size);
    
    assert(new_cap > k->capacity && "INVALID CAPACITY");
    
    a = realloc(k->a, new_cap * sizeof(*k->a));
    if (!a)
        return -errno;
    
    for (unsigned int i = k->end; i < new_end; ++i)
        a[i] = k->size + i - k->end;
    
    k->a        = a;
    k->end      = new_end;
    k->size     = new_size;
    k->capacity = new_cap;
    
    return 0;
}

void kfy_remove(struct kfy *__restrict k, unsigned int cnt)
{
    if (cnt > 0) {
        k->end  -= cnt;
        k->size -= cnt;
        
        for (unsigned int i = 0; i < k->end; ++i)
            k->a[i] = i;
    }
}

unsigned int kfy_size(const struct kfy *__restrict k)
{
    return k->size;
}

bool kfy_cycle_done(const struct kfy *__restrict k)
{
    return k->end == 0;
}

