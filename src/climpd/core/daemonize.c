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
#include <string.h>
#include <unistd.h>
// #include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
// #include <execinfo.h>

#include <libvci/macro.h>
#include <libvci/error.h>

#include <core/daemonize.h>
#include <core/climpd-log.h>

static const char *tag = "daemonize";


static int init_signal_handlers(const struct signal_handle *__restrict handles, 
                                unsigned int size)
{
    struct sigaction sa;
    int err;
    
    memset(&sa, 0, sizeof(sa));
    
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    
    for (unsigned int i = 0; i < size; ++i) {
        sa.sa_sigaction = handles[i].handler;
        
        err = sigaction(handles[i].signal, &sa, NULL);
        if (err < 0) {
            err = -errno;
            climpd_log_e(tag, "failed to set handler for signal '%s' - %s\n",
                         strsignal(handles[i].signal), errstr);
            return err;
        }
    }
    
    return 0;
}

static void close_std_streams(void)
{
    const char *path = "/dev/null"; 
    int streams[] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };
    int fd, err;
        
    fd = open(path, O_RDWR);
    if(fd < 0) {
        climpd_log_w(tag, "failed to open '%s' for redirecting - %s - "
                     "falling back to closing standard streams\n", 
                     path, errstr);
        goto backup;
    }
    
    for(unsigned int i = 0; i < ARRAY_SIZE(streams); ++i) {
        err = dup2(fd, streams[i]);
        if(err < 0) {
            climpd_log_w(tag, "failed to redirect standard stream %d - %s - "
                         "falling back to closing standard streams\n", 
                         streams[i], errstr);
            goto backup;
        }
    }
    
    close(fd);
    
    return;
    
backup:
    close(fd);
    
    for (unsigned int i = 0; i < ARRAY_SIZE(streams); ++i)
        close(streams[i]);
}

int daemonize(const struct signal_handle *__restrict handles, unsigned int size)
{
    int err;
    pid_t pid, sid;
    
    pid = fork();
    if(pid < 0)
        return -errno;
    
    if(pid > 0)
        _exit(EXIT_SUCCESS);
    
    /*
     * We don't want to get closed when our tty is closed.
     * Creating our own session will prevent this.
     */
    sid = setsid();
    if(sid < 0)
        return -errno;
    
    /* 
     * Exit session leading process:
     * Only a session leading process is able to acquire
     * a controlling terminal
     */
    pid = fork();
    if(pid < 0)
        return -errno;
    
    if(pid > 0)
        _exit(EXIT_SUCCESS);
    
    umask(0);
    
    err = chdir("/");
    if(err < 0)
        return -errno;
    
    err = init_signal_handlers(handles, size);
    if (err < 0) {
        climpd_log_e(tag, "setting signal handlers failed - %s\n", strerr(-err));
        return err;
    }
    
    close_std_streams();
    
    return 0;
}