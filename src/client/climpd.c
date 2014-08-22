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

#include <libvci/map.h>
#include <libvci/hash.h>
#include <libvci/compare.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include "../shared/ipc.h"

#include "climpd.h"

extern char **environ;

static int fd;
static struct message *msg;

static struct command_handle {
    const char *command;
    enum message_id message_id;
    bool has_arg;
    bool arg_is_path;
} cmd_handles[] = {
    { "pause",           IPC_MESSAGE_PAUSE,             false,  false   },
    { "play",            IPC_MESSAGE_PLAY,              false,  false   },
    { "stop",            IPC_MESSAGE_STOP,              false,  false   },
    { "next",            IPC_MESSAGE_NEXT,              false,  false   },
    { "previous",        IPC_MESSAGE_PREVIOUS,          false,  false   },
    
    { "discover",        IPC_MESSAGE_DISCOVER,          true,   true    },
    
    { "shutdown",        IPC_MESSAGE_SHUTDOWN,          false,  false   },
    
    { "get-colors",      IPC_MESSAGE_GET_COLORS,        false,  false   },
    { "get-config",      IPC_MESSAGE_GET_CONFIG,        false,  false   },
    { "get-files",       IPC_MESSAGE_GET_FILES,         false,  false   },
    { "get-playlist",    IPC_MESSAGE_GET_PLAYLIST,      false,  false   },
    { "get-state",       IPC_MESSAGE_GET_STATE,         false,  false   },
    { "get-volume",      IPC_MESSAGE_GET_VOLUME,        false,  false   },
    { "get-log",         IPC_MESSAGE_GET_LOG,           false,  false   },
    
    { "set-playlist",    IPC_MESSAGE_SET_PLAYLIST,      true,   true    },
    { "set-volume",      IPC_MESSAGE_SET_VOLUME,        true,   false   },
    { "set-repeat",      IPC_MESSAGE_SET_REPEAT,        true,   false   },
    { "set-shuffle",     IPC_MESSAGE_SET_SHUFFLE,       true,   false   },
    
    { "play-file",       IPC_MESSAGE_PLAY_FILE,         true,   true    },

    { "play-track",      IPC_MESSAGE_PLAY_TRACK,        true,   false   },
    
    { "load-config",     IPC_MESSAGE_LOAD_CONFIG,       true,   false   },
    { "load-media",      IPC_MESSAGE_LOAD_MEDIA,        true,   true    },
    
    { "remove-track",    IPC_MESSAGE_REMOVE_TRACK,      true,   false   },
    { "remove-playlist", IPC_MESSAGE_REMOVE_PLAYLIST,   true,   false   },
};

static struct map *cmd_map;

static int climpd_connect(void)
{
    struct sockaddr_un addr;
    int err;
    
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) {
        err = -errno;
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    ipc_message_clear(msg);
    ipc_message_set_id(msg, IPC_MESSAGE_HELLO);
    ipc_message_set_fds(msg, STDOUT_FILENO, STDERR_FILENO);
    
    err = ipc_send_message(fd, msg);
    if(err < 0) {
        fprintf(stderr, "climp: ipc_send_message(): %s\n", strerror(-err));
        goto cleanup1;
    }
    
    err = ipc_recv_message(fd, msg);
    if(err < 0) {
        fprintf(stderr, "climp: ipc_recv_message(): %s\n", strerror(-err));
        goto cleanup1;
    }
    
    if(ipc_message_id(msg) != IPC_MESSAGE_OK) {
        err = -EIO;
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
    ipc_message_clear(msg);
    ipc_message_set_id(msg, IPC_MESSAGE_GOODBYE);
    
    ipc_send_message(fd, msg);
    
    /* This step is needed for a flawless synchronisation */
    ipc_recv_message(fd, msg);

    close(fd);
}

static int spawn_climpd(void)
{
    char *name = "/usr/local/bin/climpd";
    char *argv[] = { name, NULL };
    pid_t pid;
    int status;
    
    pid = fork();
    if(pid < 0)
        return -errno;

    if(pid > 0) {
        if(waitpid(pid, &status, 1) == (pid_t) -1)
            return -errno;
        
        return 0;
    }
    
    execve(name, argv, environ);
    
    fprintf(stderr, "climp: execve(): %s\n", strerror(errno));
    
    exit(EXIT_FAILURE);
}

static int climpd_send_message(enum message_id id, const char *__restrict arg)
{
    int err;
    
    ipc_message_clear(msg);
    ipc_message_set_id(msg, id);
    
    if(arg) {
        err = ipc_message_set_arg(msg, arg);
        if(err < 0) {
            fprintf(stderr, "climp: argument '%s' - %s\n", arg, strerror(-err));
            return err;
        }
    }
    
    err = ipc_send_message(fd, msg);
    if(err < 0) {
        fprintf(stderr, "climp: sending message '%s' failed - %s\n", 
                ipc_message_id_string(id), strerror(-err));

        return err;
    }
    
    return 0;
}

int climpd_init(void)
{
    int i, attempts, err;

    msg = ipc_message_new();
    if(!msg)
        return -errno;
    
    err = climpd_connect();
    if(err < 0) {
        if(err != -ENOENT && err != -ECONNREFUSED)
            goto cleanup1;
        
        err = unlink(IPC_SOCKET_PATH);
        if(err < 0 && errno != ENOENT) {
            err = -errno;
            goto cleanup1;
        }
        
        err = spawn_climpd();
        if(err < 0)
            goto cleanup1;
        
        attempts = 1000;
        
        while(attempts--) {
            usleep(10 * 1000);
            
            err = climpd_connect();
            if(err < 0)
                continue;
            
            break;
        }
        
        if(err < 0)
            goto cleanup2;
    }
    
    cmd_map = map_new(32, &compare_string, &hash_string);
    if(!cmd_map) {
        err = -errno;
        goto cleanup2;
    }
    
    for(i = 0; i < ARRAY_SIZE(cmd_handles); ++i) {
        err = map_insert(cmd_map, cmd_handles[i].command, cmd_handles + i);
        if(err < 0)
            goto cleanup3;
    }

    return 0;

cleanup3:
    map_delete(cmd_map);
cleanup2:
    climpd_disconnect();
cleanup1:
    ipc_message_delete(msg);
    
    return err;
}

void climpd_destroy(void)
{
    map_delete(cmd_map);
    climpd_disconnect();
    ipc_message_delete(msg);
}

void climpd_handle_args(int argc, char *argv[])
{
    struct command_handle *handle;
    char *path;
    int i;
    
    for(i = 0; i < argc; ++i) {
        handle = map_retrieve(cmd_map, argv[i]);
        if(!handle) {
            fprintf(stderr, "climp: unknown command '%s'\n", argv[i]);
            continue;
        }
        
        if(!handle->has_arg) {
            climpd_send_message(handle->message_id, NULL);
            continue;
        }
        
        i += 1;
        if(i >= argc) {
            fprintf(stderr, "climp: missing argument for command '%s'\n", 
                    handle->command);
            continue;
        }
        
        if(map_contains(cmd_map, argv[i])) {
            fprintf(stderr, "climp: missing argument for command '%s'\n", 
                    handle->command);
            i -= 1;
            continue;
        }
        
        for(; i < argc; ++i) {
            if(map_contains(cmd_map, argv[i])) {
                i -= 1;
                break;
            }
            
            /* now we can be sure we got a valid argument */
            if(!handle->arg_is_path) {
                climpd_send_message(handle->message_id, argv[i]);
                continue;
            }
            
            /* 
             * The climpd needs absolute paths since it may have a different 
             * workind directory than this process
             */
            
            path = realpath(argv[i], NULL);
            if(!path) {
                fprintf(stderr, "climp: %s - %s\n", argv[i], strerror(errno));
                continue;
            }
            
            climpd_send_message(handle->message_id, path);
            
            free(path);
        }
    }
}

