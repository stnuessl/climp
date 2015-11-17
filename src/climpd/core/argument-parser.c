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

#include <libvci/hash.h>
#include <libvci/compare.h>
#include <libvci/error.h>

#include <core/climpd-log.h>
#include <core/argument-parser.h>

static const char *tag = "argument-parser";

int argument_parser_init(struct argument_parser *__restrict ap, 
                         struct arg *__restrict args,
                         unsigned int size)
{
    const struct map_config m_conf = {
        .size           = size << 2,
        .lower_bound    = MAP_DEFAULT_LOWER_BOUND,
        .upper_bound    = MAP_DEFAULT_UPPER_BOUND,
        .static_size    = false,
        .key_compare    = &compare_string,
        .key_hash       = &hash_string,
        .data_delete    = NULL,
    };
    int err;
    
    err = map_init(&ap->map, &m_conf);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize map - %s\n", strerr(-err));
        return err;
    }
    
    for (unsigned int i = 0; i < size; ++i) {
        if (*args[i].long_arg != '\0') {
            err = map_insert(&ap->map, args[i].long_arg, args + i);
            if (err < 0) {
                climpd_log_e(tag, "failed to initialize handle \"%s\" - %s\n", 
                             args[i].long_arg, strerr(-err));
                goto cleanup1;
            }
        }
        
        if (*args[i].short_arg != '\0') {
            err = map_insert(&ap->map, args[i].short_arg, args + i);
            if (err < 0) {
                climpd_log_e(tag, "failed to initialize option \"%s\" - %s\n", 
                             args[i].short_arg, strerr(-err));
                goto cleanup1;
            }
        }
    }
    
    ap->default_handler = NULL;
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;
    
cleanup1:
    map_destroy(&ap->map);
    return err;
}

void argument_parser_destroy(struct argument_parser *__restrict ap)
{
    map_destroy(&ap->map);
    climpd_log_i(tag, "destroyed\n");
}

int argument_parser_run(struct argument_parser *__restrict ap, 
                        const char **argv,
                        int argc)
{
    int err;
    
    for (int i = 0 ; i < argc; ++i) {
        struct arg *arg = map_retrieve(&ap->map, argv[i]);
        
        if (!arg) {
            climpd_log_w(tag, "skipping invalid argument '%s'\n", argv[i]);
            
            if (ap->default_handler)
                ap->default_handler(argv[i]);
            
            continue;
        }
        
        int j = i + 1;
        
        while (j < argc && !map_contains(&ap->map, argv[j]))
            j++;
        
        j--;
        
        err = arg->handler(argv[i], argv + i + 1, j - i);
        if (err < 0)
            climpd_log_w(tag, "\"%s\" failed - %s\n", argv[i], strerr(-err));
        
        i = j;
    }
    
    return 0;
}

void argument_parser_set_default_handler(struct argument_parser *__restrict ap, 
                                         void (*func)(const char *))
{
    ap->default_handler = func;
}