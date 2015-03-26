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

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdbool.h>

void die(const char *__restrict tag, const char *__restrict msg);

void die_errno(const char *__restrict tag, const char *__restrict msg, int err);

void die_fmt(const char *__restrict tag, const char *__restrict fmt, ...)
                                            __attribute__((format(printf,2,3)));

void die_failed_init(const char *__restrict tag);

const char *yes_no(bool val);

const char *true_false(bool val);

int str_to_int(const char *__restrict s, int *__restrict i);

int str_to_sec(const char *__restrict s, int *__restrict sec);

#endif /* _UTIL_H_ */