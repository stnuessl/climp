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

#ifndef _URI_H_
#define _URI_H_

char *uri_new(const char *__restrict s);

void uri_delete(char *__restrict uri);

const char *uri_hierarchical(const char *__restrict uri);

bool uri_is_file(const char *__restrict uri);

bool uri_is_http(const char *__restrict uri);

bool uri_ok(const char *__restrict uri);

#endif /* _URI_H_ */