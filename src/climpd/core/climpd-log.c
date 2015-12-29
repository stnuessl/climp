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
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

#include <libvci/log.h>
#include <libvci/error.h>

#include <core/climpd-log.h>

#define LOG_PATH_RAW "/tmp/climpd-%d.log"

static struct log log;

int climpd_log_init(const char *path)
{
    int err;

    err = log_init(&log, path, LOG_ALL);
    if (err < 0)
        return err;
    
    log_set_level(&log, LOG_DEBUG);
    
    return 0;
}

void climpd_log_destroy(void)
{
    log_destroy(&log);
}

void climpd_log_d(const char *__restrict tag, const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vprintf(&log, LOG_DEBUG, tag, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_i(const char *__restrict tag, const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vprintf(&log, LOG_INFO, tag, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_w(const char *__restrict tag, const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vprintf(&log, LOG_WARNING, tag, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_e(const char *__restrict tag, const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vprintf(&log, LOG_ERROR, tag, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_v_d(const char *__restrict tag, const char *__restrict fmt, 
                    va_list vargs)
{
    log_vprintf(&log, LOG_DEBUG, tag, fmt, vargs);
}

void climpd_log_v_i(const char *__restrict tag, const char *__restrict fmt, 
                    va_list vargs)
{
    log_vprintf(&log, LOG_INFO, tag, fmt, vargs);
}

void climpd_log_v_w(const char *__restrict tag, const char *__restrict fmt, 
                    va_list vargs)
{
    log_vprintf(&log, LOG_WARNING, tag, fmt, vargs);
}

void climpd_log_v_e(const char *__restrict tag, const char *__restrict fmt, 
                    va_list vargs)
{
    log_vprintf(&log, LOG_ERROR, tag, fmt, vargs);
}

void climpd_log_append(const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vappend(&log, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_print(int fd)
{
    log_print(&log, fd);
}

int climpd_log_fd(void)
{
    return log_fd(&log);
}