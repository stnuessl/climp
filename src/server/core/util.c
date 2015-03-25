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
#include <stdarg.h>

#include <libvci/error.h>

#include <core/climpd-log.h>

void die(const char *__restrict tag, const char *__restrict msg)
{
    climpd_log_e(tag, "%s\n", msg);
    exit(EXIT_FAILURE);
}

void die_errno(const char *__restrict tag, const char *__restrict msg, int err)
{
    climpd_log_e(tag, "%s - %s\n", msg, strerr(abs(err)));
    exit(EXIT_FAILURE);
}

void die_fmt(const char *__restrict tag, const char *__restrict fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    climpd_log_v_e(tag, fmt, vargs);
    
    va_end(vargs);
    
    exit(EXIT_FAILURE);
}

void die_failed_init(const char *__restrict tag)
{
    die(tag, "failed to initialize - aborting...");
}