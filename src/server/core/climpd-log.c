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

#define CLIMPD_LOG_FILE "/tmp/climpd.log"

static struct log *log;

void climpd_log_init(void)
{
    log = log_new(CLIMPD_LOG_FILE, LOG_ALL);
    if(!log) {
        /* last hope is to get a error message to stderr ... */
        fprintf(stderr, "climpd: failed to initialize log file - %s - "
                "aborting...\n", strerr(errno));
        exit(EXIT_FAILURE);
    }
    
    log_set_level(log, LOG_DEBUG);
}

void climpd_log_destroy(void)
{
    log_delete(log);
}

void climpd_log_d(const char *__restrict tag, const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vprintf(log, LOG_DEBUG, tag, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_i(const char *__restrict tag, const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vprintf(log, LOG_INFO, tag, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_w(const char *__restrict tag, const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vprintf(log, LOG_WARNING, tag, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_e(const char *__restrict tag, const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_vprintf(log, LOG_ERROR, tag, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_append(const char *fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    
    log_append(log, fmt, vargs);
    
    va_end(vargs);
}

void climpd_log_print(int fd)
{
    log_print(log, fd);
}

int climpd_log_fd(void)
{
    return log_fd(log);
}