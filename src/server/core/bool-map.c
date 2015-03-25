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

#include <libvci/map.h>
#include <libvci/hash.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/bool-map.h>
#include <core/util.h>

const char *tag = "bool-map";
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

void bool_map_init(void)
{
    const struct map_config map_conf = {
        .size           = 32,
        .lower_bound    = MAP_DEFAULT_LOWER_BOUND,
        .upper_bound    = MAP_DEFAULT_UPPER_BOUND,
        .static_size    = false,
        .key_compare    = &string_compare,
        .key_hash       = &hash_string,
        .data_delete    = NULL,
    };
    int err;
    
    err = map_init(&bool_map, &map_conf);
    if(err < 0) {
        climpd_log_e(tag, "failed to initialize map - %s\n", strerr(-err));
        goto fail;
    }
    
    for(unsigned int i = 0; i < ARRAY_SIZE(t); ++i) {
        err = map_insert(&bool_map, t[i], &yes);
        if(err < 0) {
            climpd_log_e(tag, "failed to initialize value \"%s\"\n", t[i]);
            goto fail;
        }
    }
    
    for(unsigned int i = 0; i < ARRAY_SIZE(f); ++i) {
        err = map_insert(&bool_map, f[i], &no);
        if(err < 0) {
            climpd_log_e(tag, "failed to initialize value \"%s\"\n", t[i]);
            goto fail;
        }
    }
    
    climpd_log_i(tag, "initialized\n");
    
    return;
    
fail:
    die_failed_init(tag);
}

void bool_map_destroy(void)
{
    map_destroy(&bool_map);
    
    climpd_log_i(tag, "destroyed\n");
}

const bool *bool_map_value(const char *key)
{
    return map_retrieve(&bool_map, key);
}

void bool_map_print(int fd)
{
    const char *p, *q;
    unsigned int size;
    
    size = max(ARRAY_SIZE(t), ARRAY_SIZE(f));
    
    dprintf(fd, " true  --> %s\t\tfalse --> %s\n", t[0], f[0]);
    
    for(unsigned int i = 1; i < size; ++i) {
        p = (i < ARRAY_SIZE(t)) ? t[i] : "";
        q = (i < ARRAY_SIZE(f)) ? f[i] : "";
     
        dprintf(fd, " %-5s --> %s\t\t%-5s --> %s\n", "", p, "", q);
    }
}