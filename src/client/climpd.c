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
#include <ctype.h>
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

#define D_PAUSE "Pause playback."
#define D_PLAY "Play next available track or resume if stopped."
#define D_PEEK "Get current stream position. Does not work if stopped."
#define D_STOP "Stop playback."
#define D_NEXT "Jump to the next title."
#define D_PREV "Jump to the previous title."
#define D_SEEK "Set the current position of the stream."
#define D_DISC "Discover playable songs inside a folder."
#define D_SHUT "Shutdown the playback process (close the application)."
#define D_GCOLOR "Get available colors for printing."
#define D_GCONF "Print the currently loaded config."
#define D_GFILES "Get the path of the files in the playlist."
#define D_GPLAYL "Print the currently played playlist."
#define D_GSTATE "Get current state of the player."
#define D_GVOL "Get the current volume."
#define D_GLOG "Display the log file."
#define D_SPLAY "Set the playlist."
#define D_SVOL "Set the volume of the player."
#define D_SREP "Enable / disable repeat."
#define D_SSHUFF "Enable / disable shuffling songs."
#define D_PLAYF "Add a file to the playlist and play it."
#define D_PLAYT "Play the song at position [arg] in the playlist."
#define D_LDCONF "Load a configuration file." 
#define D_LDMEDI "Add a song to the playlist." 
#define D_RMTR "Remove song at position [arg] in the playlist."
#define D_RMPLAY "Remove the current playlist."


static int fd;
static struct message *msg;

static struct option {
    const char *command;
    enum message_id message_id;
    bool has_arg;
    bool arg_is_path;
    const char *description;
} opts[] = {
    { "pause",           IPC_MESSAGE_PAUSE,           false,  false, D_PAUSE  },
    { "play",            IPC_MESSAGE_PLAY,            false,  false, D_PLAY   },
    { "peek",            IPC_MESSAGE_PEEK,            false,  false, D_PEEK   },
    { "stop",            IPC_MESSAGE_STOP,            false,  false, D_STOP   },
    { "next",            IPC_MESSAGE_NEXT,            false,  false, D_NEXT   },
    { "previous",        IPC_MESSAGE_PREVIOUS,        false,  false, D_PREV   },
    { "seek",            IPC_MESSAGE_SEEK,            true,   false, D_SEEK   },
    
    { "discover",        IPC_MESSAGE_DISCOVER,        true,   true,  D_DISC   },
    
    { "shutdown",        IPC_MESSAGE_SHUTDOWN,        false,  false, D_SHUT   },
    
    { "get-colors",      IPC_MESSAGE_GET_COLORS,      false,  false, D_GCOLOR },
    { "get-config",      IPC_MESSAGE_GET_CONFIG,      false,  false, D_GCONF  },
    { "get-files",       IPC_MESSAGE_GET_FILES,       false,  false, D_GFILES },
    { "get-playlist",    IPC_MESSAGE_GET_PLAYLIST,    false,  false, D_GPLAYL },
    { "get-state",       IPC_MESSAGE_GET_STATE,       false,  false, D_GSTATE },
    { "get-volume",      IPC_MESSAGE_GET_VOLUME,      false,  false, D_GVOL   },
    { "get-log",         IPC_MESSAGE_GET_LOG,         false,  false, D_GLOG   },
    
    { "set-playlist",    IPC_MESSAGE_SET_PLAYLIST,    true,   true,  D_SPLAY  },
    { "set-volume",      IPC_MESSAGE_SET_VOLUME,      true,   false, D_SVOL   },
    { "set-repeat",      IPC_MESSAGE_SET_REPEAT,      true,   false, D_SREP   },
    { "set-shuffle",     IPC_MESSAGE_SET_SHUFFLE,     true,   false, D_SSHUFF },
    
    { "play-file",       IPC_MESSAGE_PLAY_FILE,       true,   true,  D_PLAYF  },

    { "play-track",      IPC_MESSAGE_PLAY_TRACK,      true,   false, D_PLAYT  },
    
    { "load-config",     IPC_MESSAGE_LOAD_CONFIG,     true,   false, D_LDCONF },
    { "load-media",      IPC_MESSAGE_LOAD_MEDIA,      true,   true,  D_LDMEDI },
    
    { "remove-track",    IPC_MESSAGE_REMOVE_TRACK,    true,   false, D_RMTR   },
    { "remove-playlist", IPC_MESSAGE_REMOVE_PLAYLIST, true,   false, D_RMPLAY },
};

static struct map *cmd_map;

static int climpd_connect(void)
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
    
    ipc_message_clear(msg);
    ipc_message_set_id(msg, IPC_MESSAGE_HELLO);
    ipc_message_set_fds(msg, STDOUT_FILENO, STDERR_FILENO);
    
    err = ipc_send_message(fd, msg);
    if (err < 0) {
        fprintf(stderr, "climp: ipc_send_message(): %s\n", strerror(-err));
        goto cleanup1;
    }
    
    err = ipc_recv_message(fd, msg);
    if (err < 0) {
        fprintf(stderr, "climp: ipc_recv_message(): %s\n", strerror(-err));
        goto cleanup1;
    }
    
    if (ipc_message_id(msg) != IPC_MESSAGE_OK) {
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

static int climpd_send_message(enum message_id id, const char *__restrict arg)
{
    int err;
    
    ipc_message_clear(msg);
    ipc_message_set_id(msg, id);
    
    if (arg) {
        err = ipc_message_set_arg(msg, arg);
        if (err < 0) {
            fprintf(stderr, "climp: argument '%s' - %s\n", arg, strerror(-err));
            return err;
        }
    }
    
    err = ipc_send_message(fd, msg);
    if (err < 0) {
        fprintf(stderr, "climp: sending message '%s' failed - %s\n", 
                ipc_message_id_string(id), strerror(-err));

        return err;
    }
    
    return 0;
}

static void climpd_print_help(void)
{
    char *arg;
    int i;
    
    fprintf(stdout, "climp - available options:\n");
    
    for (i = 0; i < ARRAY_SIZE(opts); ++i) {
        arg = (opts[i].has_arg) ? "[arg]" : "     ";
        
        fprintf(stdout, "  %-20s %s  %s\n", 
                opts[i].command, arg, opts[i].description);
    }
}

static void to_lower_str(char *str)
{
    char *p;
    
    for (p = str; *p != '\0'; ++p)
        *p = (char) tolower(*p);
}

int climpd_init(void)
{
    const struct map_config map_conf = {
        .size           = 64,
        .lower_bound    = MAP_DEFAULT_LOWER_BOUND,
        .upper_bound    = MAP_DEFAULT_UPPER_BOUND,
        .static_size    = false,
        .key_compare    = &compare_string,
        .key_hash       = &hash_string,
        .data_delete    = NULL,
    };
    int i, attempts, err;

    msg = ipc_message_new();
    if(!msg)
        return -errno;
    
    err = climpd_connect();
    if (err < 0) {
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
        
        while (attempts--) {
            usleep(10 * 1000);
            
            err = climpd_connect();
            if(err < 0)
                continue;
            
            break;
        }
        
        if (err < 0)
            goto cleanup2;
    }
    
    cmd_map = map_new(&map_conf);
    if(!cmd_map) {
        err = -errno;
        goto cleanup2;
    }
    
    for (i = 0; i < ARRAY_SIZE(opts); ++i) {
        err = map_insert(cmd_map, opts[i].command, opts + i);
        if (err < 0)
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
    struct option *opt;
    char *path;
    int i;
    
    for (i = 0; i < argc; ++i) {
        to_lower_str(argv[i]);
        
        if (strcmp(argv[i], "help") == 0) {
            climpd_print_help();
            continue;
        }
        
        opt = map_retrieve(cmd_map, argv[i]);
        if (!opt) {
            fprintf(stderr, "climp: unknown command '%s'\n", argv[i]);
            continue;
        }

        if (!opt->has_arg) {
            climpd_send_message(opt->message_id, NULL);
            continue;
        }
        
        i += 1;
        if (i >= argc) {
            fprintf(stderr, "climp: missing argument for command '%s'\n", 
                    opt->command);
            continue;
        }
        
        if (map_contains(cmd_map, argv[i])) {
            fprintf(stderr, "climp: missing argument for command '%s'\n", 
                    opt->command);
            i -= 1;
            continue;
        }
        
        for(; i < argc; ++i) {
            if (map_contains(cmd_map, argv[i])) {
                i -= 1;
                break;
            }
            
            /* now we can be sure we got a valid argument */
            if (!opt->arg_is_path) {
                climpd_send_message(opt->message_id, argv[i]);
                continue;
            }
            
            /* 
             * The climpd needs absolute paths since it may have a different 
             * working directory than this process
             */
            
            path = realpath(argv[i], NULL);
            if (!path) {
                fprintf(stderr, "climp: %s - %s\n", argv[i], strerror(errno));
                continue;
            }
            
            climpd_send_message(opt->message_id, path);
            
            free(path);
        }
    }
}

