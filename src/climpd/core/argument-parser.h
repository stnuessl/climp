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

#ifndef _ARGUMENT_PARSER_H_
#define _ARGUMENT_PARSER_H_

#include <libvci/map.h>

struct arg {
    const char *long_arg;
    const char *short_arg;
    
    int (*handler)(const char *arg, const char **argv, int argc);
};

struct argument_parser {
    struct map map;
    
    void (*default_handler)(const char *);
};

int argument_parser_init(struct argument_parser *__restrict ap, 
                    struct arg *__restrict args,
                    unsigned int size);

void argument_parser_destroy(struct argument_parser *__restrict ap);

int argument_parser_run(struct argument_parser *__restrict ap, 
                        const char **argv,
                        int argc);

void argument_parser_set_default_handler(struct argument_parser *__restrict ap, 
                                         void (*func)(const char *));

#endif /* _ARGUMENT_PARSER_H_ */