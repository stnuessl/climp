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
#include <libvci/hash.h>
#include <libvci/macro.h>

#include "terminal-color-map.h"

static char *color_table[] = {
    "default",          COLOR_CODE_DEFAULT,
    "red",              COLOR_CODE_RED,
    "green",            COLOR_CODE_GREEN,
    "orange",           COLOR_CODE_ORANGE,
    "blue",             COLOR_CODE_BLUE,
    "magenta",          COLOR_CODE_MAGENTA,
    "cyan",             COLOR_CODE_CYAN,
    "white",            COLOR_CODE_WHITE
};

static struct map color_map;

static int string_compare(const void *a, const void *b)
{
    return strcasecmp(a, b);
}

int terminal_color_map_init(void)
{
    int i, err;
    
    err = map_init(&color_map, 32, &string_compare, &hash_string);
    if(err < 0)
        return err;
    
    for(i = 0; i < ARRAY_SIZE(color_table); i += 2) {
        err = map_insert(&color_map, color_table[i], color_table[i + 1]);
        if(err < 0) {
            map_destroy(&color_map);
            return err;
        }
    }
    
    return 0;
}

void terminal_color_map_destroy(void)
{
    map_destroy(&color_map);
}

const char *terminal_color_map_color_code(const char *key)
{
    return map_retrieve(&color_map, key);
}

void terminal_color_map_print(int fd)
{
    struct entry *e;
    const char *key, *val;
    
    map_for_each(&color_map, e) {
        key = entry_key(e);
        val = entry_data(e);
        
        dprintf(fd, "%s %s\n" COLOR_CODE_DEFAULT, val, key);
    }
}