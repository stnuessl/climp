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

#ifndef _CLIMPD_LOG_H_
#define _CLIMPD_LOG_H_

#include <stdarg.h>

int climpd_log_init(const char *path);

void climpd_log_destroy(void);

__attribute__((format(printf,2,3)))
void climpd_log_d(const char *__restrict tag, const char *fmt, ...);

__attribute__((format(printf,2,3)))
void climpd_log_i(const char *__restrict tag, const char *fmt, ...);

__attribute__((format(printf,2,3)))
void climpd_log_w(const char *__restrict tag, const char *fmt, ...);

__attribute__((format(printf,2,3)))
void climpd_log_e(const char *__restrict tag, const char *fmt, ...);

void climpd_log_v_d(const char *__restrict tag, const char *__restrict fmt, 
                    va_list vargs);

void climpd_log_v_i(const char *__restrict tag, const char *__restrict fmt, 
                    va_list vargs);

void climpd_log_v_w(const char *__restrict tag, const char *__restrict fmt, 
                    va_list vargs);

void climpd_log_v_e(const char *__restrict tag, const char *__restrict fmt, 
                    va_list vargs);

__attribute__((format(printf,1,2)))
void climpd_log_append(const char *fmt, ...);

void climpd_log_print(int fd);

int climpd_log_fd(void);

#endif /* _CLIMPD_LOG_H_ */