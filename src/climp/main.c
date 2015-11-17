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
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>

#include <libvci/error.h>

#include "../shared/ipc.h"

extern char **environ;

static int _ipc_sock;

static int connect_to_daemon(const char *__restrict path)
{
    struct sockaddr_un addr;
    int err;
    
    _ipc_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_ipc_sock < 0) {
        err = -errno;
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    
    err = connect(_ipc_sock, (struct sockaddr *) &addr, sizeof(addr));
    if (err < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    return 0;
    
cleanup1:
    close(_ipc_sock);
out:
    return err;
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

int main(int argc, char *argv[])
{
    char *sock_path = NULL;
    int fd0, fd1, fd2, attempts, status, err;
    const char *cwd = getenv("PWD");
    
    if(getuid() == 0) {
        fprintf(stderr, "climp: cannot run as root\n");
        exit(EXIT_FAILURE);
    }
    
    err = asprintf(&sock_path, "/tmp/.climpd-%d.sock", getuid());
    if (err < 0) {
        fprintf(stderr, "failed to create socket path\n");
        exit(EXIT_FAILURE);
    }

    err = connect_to_daemon(sock_path);
    if (err < 0) {
        if(err != -ENOENT && err != -ECONNREFUSED) {
            fprintf(stderr, "failed to connect to server - %s\n", strerr(-err));
            exit(EXIT_FAILURE);
        }
        
        err = unlink(sock_path);
        if(err < 0 && errno != ENOENT) {
            fprintf(stderr, "failed to cleanup old socket - %s\n", errstr);
            exit(EXIT_FAILURE);
        }
        
        err = spawn_climpd();
        if(err < 0) {
            fprintf(stderr, "failed to spawn daemon - %s\n", strerr(-err));
            exit(EXIT_FAILURE);
        }
        
        attempts = 1000;
        
        while (attempts--) {
            usleep(10 * 1000);
            
            err = connect_to_daemon(sock_path);
            if(err < 0)
                continue;
            
            break;
        }
        
        if (err < 0) {
            fprintf(stderr, "failed to connect to daemon - %s\n", strerr(-err));
            exit(EXIT_FAILURE);
        }
    }
    
    fd0 = STDIN_FILENO;
    fd1 = STDOUT_FILENO;
    fd2 = STDERR_FILENO;
    
    err = ipc_send_setup(_ipc_sock, fd0, fd1, fd2, cwd);
    if (err < 0) {
        fprintf(stderr, "failed to send environment - %s\n", strerr(-err));
        exit(EXIT_FAILURE);
    }
    
    err = ipc_send_argv(_ipc_sock, (const char **) argv + 1, argc - 1);
    if (err < 0) {
        fprintf(stderr, "failed to send commands - %s\n", strerr(-err));
        exit(EXIT_FAILURE);
    }
        
    /* This step is needed for a flawless synchronisation */
    err = ipc_recv_status(_ipc_sock, &status);
    if (err < 0) {
        fprintf(stderr, "failed to receive response - %s\n", strerr(-err));
        exit(EXIT_FAILURE);
    } else if (status) {
        fprintf(stderr, "server sent error: %s\n", strerr(status));
    }
    
    close(_ipc_sock);
    free(sock_path);
    
    return EXIT_SUCCESS;
}