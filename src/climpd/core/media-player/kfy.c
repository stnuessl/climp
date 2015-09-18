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

#include <libvci/random.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/util.h>

#include <obj/kfy.h>

#define KFY_MIN_CAPACITY 8

static const char *tag = "kfy-shuffle";
static struct random _rand;
static unsigned int *_a;
static unsigned int _end;
static unsigned int _size;
static unsigned int _capacity;

static unsigned int next_pow_2(unsigned int val)
{
    unsigned int ret = KFY_MIN_CAPACITY;
    
    while (ret <= val)
        ret <<= 1;
    
    return ret;
}

void kfy_init(unsigned int size)
{
    unsigned int cap;
    int err;
    
    err = random_init(&_rand);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize pseudo random number generator"
                     " - %s\n", strerr(-err));
        goto fail;
    }
    
    cap = next_pow_2(size);
    
    _a = malloc(cap * sizeof(*_a));
    if (!_a) {
        climpd_log_e(tag, "failed to allocate memory - %s\n", errstr);
        goto fail;
    }
    
    for (unsigned int i = 0; i < size; ++i)
        _a[i] = i;
    
    _end      = size;
    _size     = size;
    _capacity = cap;
    
    return;
    
fail:
    die_failed_init(tag);
}

void kfy_destroy(void)
{
    free(_a);
    random_destroy(&_rand);
}

void kfy_reset(void)
{
    if (_size < (_capacity >> 2) && _size >= KFY_MIN_CAPACITY) {
        unsigned int new_cap, *a;
        
        new_cap = next_pow_2(_size);
        
        assert(new_cap < _capacity && "INVALID CAPACITY");
        
        a = realloc(_a, new_cap * sizeof(*_a));
        if (a) {
            _a        = a;
            _capacity = new_cap;
        }
    }
    
    for (unsigned int i = 0; i < _size; ++i)
        _a[i] = i;
    
    _end = _size;
}

unsigned int kfy_shuffle(void)
{
    unsigned int val, index;
    
    if (_end == 0)
        kfy_reset();
    
    _end -= 1;
    
    index = random_uint_range(&_rand, 0, _end);
    
    val = _a[index];
    _a[index] = _a[k->end];
    
    return val;
}

int kfy_add(unsigned int cnt)
{
    unsigned int new_size, new_cap, new_end, *a;
    
    new_end  = _end  + cnt;
    new_size = _size + cnt;
    
    if (new_size < _capacity) {
        for (unsigned int i = _end; i < new_end; ++i)
            _a[i] = _size + i - _end;

        _size = new_size;
        _end  = new_end;
        
        return 0;
    }
    
    new_cap = next_pow_2(new_size);
    
    assert(new_cap > k->capacity && "INVALID CAPACITY");
    
    a = realloc(_a, new_cap * sizeof(*_a));
    if (!a)
        return -errno;
    
    for (unsigned int i = _end; i < new_end; ++i)
        a[i] = _size + i - _end;
    
    _a        = a;
    _end      = new_end;
    _size     = new_size;
    _capacity = new_cap;
    
    return 0;
}

void kfy_remove(unsigned int cnt)
{
    assert(cnt <= _size && "ARGUMENT 'CNT' TOO BIG");
    
    if (cnt > 0) {
        _end = (_end >= cnt) ? _end - cnt : 0;
        _size -= cnt;
        
        for (unsigned int i = 0; i < _end; ++i)
            _a[i] = i;
    }
}

unsigned int kfy_size(void)
{
    return _size;
}

bool kfy_cycle_done(void)
{
    return _end == 0;
}

