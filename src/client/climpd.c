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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libvci/error.h>

#include "../shared/ipc.h"

#include "climpd.h"

extern char **environ;

static int fd;

static int climpd_connect(const char *__restrict cwd)
{
    struct sockaddr_un addr;
    int err;
    
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        err = -errno;
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
    if (err < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    err = ipc_send_setup(fd, STDOUT_FILENO, STDERR_FILENO, cwd);
    if (err < 0) {
        fprintf(stderr, "climp: ipc_send_fds(): %s\n", strerr(-err));
        goto cleanup1;
    }
    
    return 0;
    
cleanup1:
    close(fd);
out:
    return err;
}

static void climpd_disconnect(void)
{
    int status, err;
    
    /* This step is needed for a flawless synchronisation */
    err = ipc_recv_status(fd, &status);
    if (err < 0)
        fprintf(stderr, "climp: ipc_recv_status(): %s\n", strerr(-err));
    else if (status)
        fprintf(stderr, "climp: server sent error: %s\n", strerr(status));

    close(fd);
}

static int spawn_climpd(void)
{
    char *name = "/usr/local/bin/climpd";
    char *argv[] = { name, NULL };
    pid_t pid, x;
    int status;
    
    pid = fork();
    if(pid < 0)
        return -errno;

    if (pid > 0) {
        /* 
         * The child process gets deamonized. During this process
         * it will fork its own child and then exits.
         * This is a good way to check if everything is ok
         * and give the child some time to initialize
         * (although this is not necessary)
         */
        do {
            x = waitpid(pid, &status, 1);
        } while (x == (pid_t) - 1 && errno == EINTR);
        
        if (x == (pid_t) -1)
            return -errno;
        
        return 0;
    }
    
    execve(name, argv, environ);
    
    fprintf(stderr, "climp: execve(): %s\n", strerror(errno));
    
    exit(EXIT_FAILURE);
}

int climpd_init(void)
{
    int attempts, err;
    const char *cwd = getenv("PWD");
    
    if (!cwd) {
        fprintf(stderr, "climp: unable to determine current working directy\n");
        return -ENOENT;
    }
    
    err = climpd_connect(cwd);
    if (err < 0) {
        if(err != -ENOENT && err != -ECONNREFUSED)
            return err;
        
        err = unlink(IPC_SOCKET_PATH);
        if(err < 0 && errno != ENOENT)
            return -errno;
        
        err = spawn_climpd();
        if(err < 0)
            return err;
        
        attempts = 1000;
        
        while (attempts--) {
            usleep(10 * 1000);
            
            err = climpd_connect(cwd);
            if(err < 0)
                continue;
            
            break;
        }
        
        if (err < 0)
            return err;
    }
    
    return 0;
}

void climpd_destroy(void)
{
    climpd_disconnect();
}

void climpd_handle_args(int argc, const char *argv[])
{
    int err;
    
    err = ipc_send_argv(fd, argv, argc);
    if (err < 0)
        fprintf(stderr, "climp: unable to send to server: %s\n", strerr(-err));
}

