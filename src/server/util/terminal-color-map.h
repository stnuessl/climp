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

#ifndef _TERMINAL_COLOR_MAP_H_
#define _TERMINAL_COLOR_MAP_H_

#define COLOR_CODE_DEFAULT  "\x1B[0m"
#define COLOR_CODE_RED      "\x1B[31m"
#define COLOR_CODE_GREEN    "\x1B[32m"
#define COLOR_CODE_ORANGE   "\x1B[33m"
#define COLOR_CODE_BLUE     "\x1B[34m"
#define COLOR_CODE_MAGENTA  "\x1B[35m"
#define COLOR_CODE_CYAN     "\x1B[36m"
#define COLOR_CODE_WHITE    "\x1B[37m"

int terminal_color_map_init(void);

void terminal_color_map_destroy(void);

const char *terminal_color_map_color_code(const char *key);

void terminal_color_map_print(int fd);

#endif /* _TERMINAL_COLOR_MAP_H_ */