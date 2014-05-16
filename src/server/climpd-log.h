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

int climpd_log_init(void);

void climpd_log_destroy(void);

void climpd_log_d(const char *fmt, ...);

void climpd_log_i(const char *fmt, ...);

void climpd_log_m(const char *fmt, ...);

void climpd_log_w(const char *fmt, ...);

void climpd_log_c(const char *fmt, ...);

void climpd_log_e(const char *fmt, ...);

#endif /* _CLIMPD_LOG_H_ */