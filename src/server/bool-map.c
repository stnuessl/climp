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
#include <stdio.h>
#include <string.h>

#include <libvci/map.h>
#include <libvci/macro.h>

static struct map bool_map;

bool yes = true;
bool no  = false;

static const char *t[] = {
    "true",
    "yes",
    "on",
    "y",
    "1",
};

static const char *f[] = {
    "false",
    "no",
    "off",
    "n",
    "0",
};

static int string_compare(const void *a, const void *b)
{
    return strcasecmp(a, b);
}

int bool_map_init(void)
{
    int i, err;
    
    err = map_init(&bool_map, 32, &string_compare, &hash_string);
    if(err < 0)
        return err;
    
    for(i = 0; i < ARRAY_SIZE(t); ++i) {
        err = map_insert(&bool_map, t[i], &yes);
        if(err < 0)
            goto fail;
    }
    
    for(i = 0; i < ARRAY_SIZE(f); ++i) {
        err = map_insert(&bool_map, f[i], &no);
        if(err < 0)
            goto fail;
    }
    
    return 0;
    
fail:
    map_destroy(&bool_map);
    return err;
}

void bool_map_destroy(void)
{
    map_destroy(&bool_map);
}

const bool *bool_map_value(const char *key)
{
    return map_retrieve(&bool_map, key);
}

void bool_map_print(int fd)
{
    struct entry *e;
    const char *key, *val;
    
    map_for_each(&bool_map, e) {
        key = entry_key(e);
        val = entry_data(e);
        
        dprintf(fd, " %s --> %s\n", key, (*val) ? "true" : "false");
    }
}