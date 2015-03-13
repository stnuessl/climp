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
#include <signal.h>
#include <fcntl.h>
#include <stdint.h>
#include <limits.h>
#include <locale.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <assert.h>

#include <gst/gst.h>

#include <libvci/map.h>
#include <libvci/hash.h>
#include <libvci/compare.h>
#include <libvci/clock.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include "../shared/ipc.h"
#include "../shared/constants.h"

#include "util/climpd-log.h"
#include "util/bool-map.h"
#include "util/terminal-color-map.h"

#include <core/climpd-player.h>
#include <core/media-discoverer.h>
#include <core/climpd-config.h>
#include <core/media-list.h>

/*
 * TODO:        playlist file filder under .config/tq/playlists
 * TODO:        autoload last playlist
 * TODO:        cleanup climpd-config
 */

static int handle_add(const char **argv, int argc);
static int handle_clear(const char **argv, int argc);
static int handle_config(const char **argv, int argc);
static int handle_discover(const char **argv, int argc);
static int handle_files(const char **argv, int argc);
static int handle_help(const char **argv, int argc);
static int handle_playlist(const char **argv, int argc);
static int handle_log(const char **argv, int argc);
static int handle_mute(const char **argv, int argc);
static int handle_next(const char **argv, int argc);
static int handle_pause(const char **argv, int argc);
static int handle_play(const char **argv, int argc);
static int handle_prev(const char **argv, int argc);
static int handle_quit(const char **argv, int argc);
static int handle_remove(const char **argv, int argc);
static int handle_repeat(const char **argv, int argc);
static int handle_seek(const char **argv, int argc);
static int handle_shuffle(const char **argv, int argc);
static int handle_stop(const char **argv, int argc);
static int handle_volume(const char **argv, int argc);

static const char *tag = "main";

static bool no_daemon;
struct climpd_player player;
struct media_discoverer disc;
struct clock timer;
struct climpd_config *conf;
GMainLoop *main_loop;
GIOChannel *io_server;

static int fd_out;
static int fd_err;

static struct map options_map;

struct option {
    const char *l_opt;
    const char *s_opt;
    int (*handler)(const char **argv, int argc);
};

static struct option options[] = {
    { "--add",          "-a",   &handle_add             },
    { "--clear",        "",     &handle_clear           },
    { "--config",       "",     &handle_config          },
    { "--remove",       "",     &handle_remove          },
    { "--pause",        "",     &handle_pause           },
    { "--playlist",     "",     &handle_playlist        },
    { "--repeat",       "",     &handle_repeat          },
    { "--shuffle",      "",     &handle_shuffle         },
    { "--volume",       "-v",   &handle_volume          },
    { "--quit",         "-q",   &handle_quit            },
    { "--help",         "",     &handle_help            },
    { "--previous",     "",     &handle_prev            },
    { "--next",         "-n",   &handle_next            },
    { "--play",         "-p",   &handle_play            },
    { "--files",        "",     &handle_files           },
    { "--log",          "",     &handle_log             },
    { "--mute",         "-m",   &handle_mute            },
    { "--seek",         "",     &handle_seek            },
    { "--discover",     "",     &handle_discover        },
    { "--stop",         "",     &handle_stop            },
};

static const char help[] = {
    "Usage:\n"
    "climp --cmd1 [[arg1] ...] --cmd2 [[arg1] ...]\n\n"
    "  -a, --add       \tadd a directory, .m3u, .txt and / or media file to\n"
    "                  \tthe playlist\n"
    "      --clear     \tclear the current playlist\n"
    "      --config    \tprint the climpd configuration\n"
    "      --remove    \tremove a media file from the playlist\n"
    "      --pause     \t\n" 
    "      --playlist  \t\n"
    "      --repeat    \t\n"
    "      --shuffle   \t\n"
    "  -v, --volume    \t\n"
    "  -q, --quit      \t\n"
    "      --help      \t\n"
    "      --previous  \t\n"
    "      --next      \t\n"
    "  -p, --play      \t\n"
    "      --files     \t\n"
    "      --log       \t\n"
    "      --mute      \t\n"
    "      --seek      \t\n"
    "      --discover  \t\n"
    "      --stop      \t\n"
};

static void report_invalid_boolean(const char *__restrict invalid_val)
{
    dprintf(fd_err, "climpd: unrecognized boolean value '%s'\n", invalid_val);
}

static void report_invalid_integer(const char *__restrict invalid_val)
{
    dprintf(fd_err, "climpd: unrecognized integer value '%s'\n", invalid_val);
}

// static void report_invalid_path(const char *__restrict path, int err)
// {
//     if (err == 0)
//         dprintf(fd_err, "climpd: invalid path '%s'\n", path);
//     else
//         dprintf(fd_err, "climpd: invalid path '%s' - %s\n", path, strerr(err));
// }

static void report_missing_arg(const char *__restrict cmd)
{
    dprintf(fd_err, "climpd: \"%s\" is missing at least one argument\n", cmd);
}

static void report_arg_error(const char *__restrict cmd, 
                             const char *__restrict arg, 
                             int err)
{
    dprintf(fd_err, "climpd: %s: \"%s\" - %s\n", cmd, arg, strerr(-err));
}

static void report_error(const char *__restrict cmd, 
                         const char *__restrict msg,
                         int err)
{
    dprintf(fd_err, "climpd: %s: %s - %s\n", cmd, msg, strerr(-err));
}

static void report_invalid_time_format(const char *__restrict cmd, 
                                       const char *__restrict arg)
{
    dprintf(fd_err, "climpd: %s: \"%s\" - invalid time format, "
            "use m:ss, m.ss, m,ss or just s\n", cmd, arg);
}

static void report_redundant_if_applicable(const char **argv, int argc)
{
    if (argc <= 0)
        return;
    
    dprintf(fd_err, "climpd: ignoring redundant arguments - ");
    
    for (int i = 0; i < argc; ++i)
        dprintf(fd_err, "%s ", argv[i]);
    
    dprintf(fd_err, "\n");
}

static int string_to_integer(const char *s, int *i)
{
    char *end;
    
    errno = 0;
    *i = (int) strtol(s, &end, 10);
    
    if (errno != 0)
        return -errno;
    
    if (*end != '\0')
        return -EINVAL;
    
    return 0;
}

static int make_arg_insertion(struct media_list *__restrict ml, 
                              const char *__restrict arg)
{
    struct stat st;
    int err;
    
    err = stat(arg, &st);
    if (err < 0)
        return -errno;
    
    switch (st.st_mode & S_IFMT) {
    case S_IFREG:
        if (media_discoverer_file_is_playable(&disc, arg))
            return media_list_emplace_back(ml, arg);
        else 
            return media_list_add_from_file(ml, arg);
    case S_IFDIR:
        return media_discoverer_scan_dir(&disc, arg, ml);
    default:
        return -EMEDIUMTYPE;
    }
}

static int handle_add(const char **argv, int argc)
{
    struct media_list ml;
    int err;
    
    if (argc == 0) {
        report_missing_arg("--add");
        return -EINVAL;
    }
    
    err = media_list_init(&ml);
    
    for (int i = 0; i < argc; ++i) {
        err = make_arg_insertion(&ml, argv[i]);
        if (err < 0) {
            report_arg_error("--add", argv[i], err);
            goto cleanup1;
        }
    }
    
    if (media_list_empty(&ml))
    
    
cleanup1:
    media_list_destroy(&ml);
    
    return 0;
}

static int handle_clear(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    climpd_player_clear_playlist(&player);
    
    return 0;
}

static int handle_config(const char **argv, int argc)
{
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        climpd_config_print(conf, fd_out);
        return 0;
    }
    
    /* TODO: reload config from 'argv[0]' */
        
    return 0;
}

static int handle_discover(const char **argv, int argc)
{
    struct media_list ml;
    int err;
    
    if (argc == 0) {
        report_missing_arg("--discover");
        return -EINVAL;
    }
    
    err = media_list_init(&ml);
    if (err < 0)
        return err;
    
    for (int i = 0; i < argc; ++i) {
        err = media_discoverer_scan_all(&disc, argv[i], &ml);
        if (err < 0) {
            dprintf(fd_err, "climpd: failed to scan \"%s\" - %s\n", 
                    argv[i], strerr(-err));
            goto out;
        }
    }
    
    for (unsigned int i = 0, size = media_list_size(&ml); i < size; ++i) {
        struct media *m = media_list_at(&ml, i);
        
        dprintf(fd_out, "%s\n", media_path(m));
        
        media_unref(m);
    }
    
out:
    media_list_destroy(&ml);
    
    return err;
}

static int handle_files(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    climpd_player_print_files(&player, fd_out);
    
    return 0;
}

static int handle_help(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    dprintf(fd_out, "%s\n", help);
    
    return 0;
}

static int handle_playlist(const char **argv, int argc)
{
    struct media_list ml;
    int err;
    
    if (argc == 0) {
        climpd_player_print_playlist(&player, fd_out);
        return 0;
    }
    
    err = media_list_init(&ml);
    if (err < 0) {
        dprintf(fd_err, "climpd: creating media list failed - %s\n", 
                strerr(-err));
        return err;
    }
    
    for (int i = 0; i < argc; ++i) {
        err = make_arg_insertion(&ml, argv[i]);
        if (err < 0) {
            report_arg_error("--playlist", argv[i], err);
            goto cleanup1;
        }
    }
    
    err = climpd_player_set_media_list(&player, &ml);
    if (err < 0) {
        dprintf(fd_err, "climpd: setting new media list failed - %s :: "
                "climpd-player might have lost all files\n", strerr(-err));
        return err;
    }
    
cleanup1:
    media_list_destroy(&ml);
    
    return err;
}

static int handle_log(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    climpd_log_print(fd_out);
    
    return 0;
}

static int handle_mute(const char **argv, int argc)
{
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        climpd_player_toggle_muted(&player);        
        return 0;
    }
    
    const bool *muted = bool_map_value(argv[0]);
    if (!muted) {
        report_invalid_boolean(argv[0]);
        return -EINVAL;
    }
    
    climpd_player_set_muted(&player, *muted);
    
    return 0;
}

static int handle_next(const char **argv, int argc)
{
    int err;
    
    report_redundant_if_applicable(argv, argc);
    
    err = climpd_player_next(&player);
    if (err < 0) {
        report_error("--next", "failed to play next track", err);
        return err;
    }
    
    if (climpd_player_is_stopped(&player))
        dprintf(fd_out, "climpd: --next: finished playback\n");
    
    return 0;
}

static int handle_pause(const char **argv, int argc)
{
    int err;
    
    report_redundant_if_applicable(argv, argc);

    switch (climpd_player_state(&player)) {
    case CLIMPD_PLAYER_PLAYING:
        climpd_player_pause(&player);
        break;
    case CLIMPD_PLAYER_PAUSED:
        err = climpd_player_play(&player);
        if (err < 0) {
            report_error("--pause", "unable to resume playing", err);
            return err;
        }
        break;
    case CLIMPD_PLAYER_STOPPED:
        dprintf(fd_out, "climpd: --pause: player is stopped - done\n");
        break;
    default:
        break;
    }
        
    return 0;
}

static int handle_play(const char **argv, int argc)
{
    struct media_list ml;
    int index;
    bool valid_index;
    int err;
    
    if (argc == 0)
        return climpd_player_play(&player);
    
    err = media_list_init(&ml);
    if (err < 0) {
        report_arg_error("--play", argv[0], err);
        return err;
    }
    
    valid_index = false;
    
    for (int i = 0; i < argc; ++i) {
        err = string_to_integer(argv[i], &index);
        if (err == 0) {
            valid_index = true;
            continue;
        }
        
        err = make_arg_insertion(&ml, argv[i]);
        if (err < 0) {
            report_arg_error("--play", argv[i], err);
            goto cleanup1;
        }
    }
    
    if (!media_list_empty(&ml)) {
        /* 
         * Run this first so a wider range of indices become valid
         * especially usefull if loading first playlist, e.g.:
         * 
         *     climp --clear --play my_playlist.m3u 10
         */
        err = climpd_player_add_media_list(&player, &ml);
        if (err < 0) {
            report_error("--play", "adding files to player failed", err);
            goto cleanup1;
        }    
    }
    
    if (valid_index) {
        err = climpd_player_play_track(&player, index);
        if (err < 0)
            report_error("--play", "failed to play specified index", err);
    } else if (!media_list_empty(&ml)) {
        struct media *m = media_list_at(&ml, 0);
        
        err = climpd_player_play_path(&player, media_path(m));
        if (err < 0) {
            report_error("--play", "failed to play first file of added "
                         "playlist", err);
        }
        
        media_unref(m);
    } else {
        dprintf(fd_err, "climpd: --play: nothing to do - maybe an empty "
                "playlist file was loaded\n");
    }
    
cleanup1:
    media_list_destroy(&ml);
    
    return err;
}

static int handle_prev(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
        
    assert(0 && "NOT IMPLEMENTED");
    
    return 0;
}

static int handle_quit(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    g_main_loop_quit(main_loop);
    
    return 0;
}

static int handle_remove(const char **argv, int argc)
{
    struct media_list ml;
    int err;
    
    err = media_list_init(&ml);
    
    for (int i = 0; i < argc; ++i) {
        int index;
        
        err = string_to_integer(argv[i], &index);
        if (err == 0) {
            climpd_player_delete_index(&player, index);
            continue;
        }
        
        err = make_arg_insertion(&ml, argv[i]);
        if (err < 0) {
            report_arg_error("--remove", argv[i], err);
            goto cleanup1;
        }
    }
    
    climpd_player_remove_media_list(&player, &ml);

cleanup1:
    media_list_destroy(&ml);
    
    return 0;
}

static int handle_repeat(const char **argv, int argc)
{
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        bool repeat = climpd_player_toggle_repeat(&player);
        
        dprintf(fd_out, "  Repeating is now %s\n", (repeat) ? "ON" : "OFF");
        return 0;
    }
    
    const bool *repeat = bool_map_value(argv[0]);
    if (!repeat) {
        report_invalid_boolean(argv[0]);
        return -EINVAL;
    }
    
    climpd_player_set_repeat(&player, *repeat);
    
    return 0;
}

static int handle_seek(const char **argv, int argc)
{
    char *r;
    long val;
    int err;
 
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        int val = climpd_player_peek(&player);
        
        int min = val / 60;
        int sec = val % 60;
        
        dprintf(fd_out, "  %d:%02d\n", min, sec);
        return 0;
    }

    errno = 0;
    val = strtol(argv[0], &r, 10);
    if (errno) {
        err = -errno;
        report_arg_error("--seek", argv[0], err);
        return err;
    }
    
    if (*r != '\0') {
        if (*r != ':' && *r != '.' && *r != ',' && *r != ' ') {
            report_invalid_time_format("--seek", argv[0]);
            return -EINVAL;
        }
        
        val *= 60;
        errno = 0;
        
        val += strtol(r + 1, &r, 10);
        if (errno) {
            err = -errno;
            report_arg_error("--seek", argv[0], err);
            return err;
        } else if (*r != '\0') {
            report_invalid_time_format("--seek", argv[0]);
            return -EINVAL;
        }
    }
    
    err = climpd_player_seek(&player, val);
    if(err < 0) {
        report_error("--seek", "seeking to position failed", err);
        return err;
    }

    return 0;
}



static int handle_shuffle(const char **argv, int argc)
{
    report_redundant_if_applicable(argv + 1, argc - 1);

    if (argc == 0) {
        bool shuffle = climpd_player_toggle_shuffle(&player);        
        
        dprintf(fd_out, "  Shuffling is now %s\n", (shuffle) ? "ON" : "OFF");
        return 0;
    } 
    
    const bool *shuffle = bool_map_value(argv[0]);
    if (!shuffle) {
        report_invalid_boolean(argv[0]);
        return -EINVAL;
    }
    
    climpd_player_set_shuffle(&player, *shuffle);
    
    return 0;
}

static int handle_stop(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    climpd_player_stop(&player);
    
    return 0;
}

static int handle_volume(const char **argv, int argc)
{
    int vol, err;
    
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        climpd_player_print_volume(&player, fd_out);
        return 0;
    }
    
    err = string_to_integer(argv[0], &vol);
    if (err < 0) {
        report_invalid_integer(argv[0]);
        return -EINVAL;
    }

    climpd_player_set_volume(&player, (unsigned int) abs(vol));
    
    return 0;
}

static int handle_argv(const char **argv, int argc)
{
    int err;
    
    for (int i = 0 ; i < argc; ++i) {
        struct option *opt = map_retrieve(&options_map, argv[i]);
        
        if (opt) {
            int j = i + 1;
            
            while (j < argc && !map_contains(&options_map, argv[j]))
                j++;
            
            j--;
            
            err = opt->handler(argv + i + 1, j - i);
            if (err < 0) { 
                climpd_log_w(tag, "handling option \"%s\" failed - %s\n",
                             argv[i], strerr(-err));
            }
            
            i = j;
        } else {
            dprintf(fd_err, "climpd: invalid option \"%s\"\n", argv[i]);
        }
    }
    
    return 0;
}

static int handle_connection(int fd)
{
    char **argv, *cwd;
    int argc, err;
    
    err = ipc_recv_setup(fd, &fd_out, &fd_err, &cwd);
    if (err < 0) {
        climpd_log_e(tag, "receiving client's fds failed - %s\n", strerr(-err));
        return err;
    }
    
    err = ipc_recv_argv(fd, &argv, &argc);
    if (err < 0) {
        climpd_log_e(tag, "receiving arguments failed - %s\n", strerr(-err));
        goto cleanup1;
    }
    
    err = chdir(cwd);
    if (err < 0) {
        climpd_log_w(tag, "changing working directory to '%s' failed - %s\n", 
                     cwd, errstr);
    }
    
    err = handle_argv((const char **) argv, argc);
    
    chdir("/");
    
    if (err < 0) {
        climpd_log_e(tag, "handling arguments failed - %s\n", strerr(-err));
        goto cleanup2;
    }
    
    err = ipc_send_status(fd, err);
    if (err < 0)
        climpd_log_e(tag, "sending response failed - %s\n", strerr(-err));
    
cleanup2:
    free(argv);
cleanup1:
    free(cwd);
    close(fd_err);
    close(fd_out);
    
    return err;
}

static gboolean handle_server_fd(GIOChannel *src, GIOCondition cond, void *data)
{
    struct ucred creds;
    socklen_t cred_len;
    int fd, err;
    
    (void) src;
    (void) data;
    (void) cond;
    
    clock_reset(&timer);
    
    fd = accept(g_io_channel_unix_get_fd(io_server), NULL, NULL);
    if(fd < 0) {
        err = -errno;
        goto out;
    }
    
    cred_len = sizeof(creds);
    
    err = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &creds, &cred_len);
    if(err < 0) {
        err = -errno;
        climpd_log_e(tag, "getsockopt(): %s\n", strerr(errno));
        goto out;
    }
    
    if(creds.uid != getuid()) {
        climpd_log_w(tag, "non-authorized user %d connected -> closing "
        "connection\n", creds.uid);
        err = -EPERM;
        goto out;
    }
    
    climpd_log_i(tag, "user %d connected on socket %d\n", creds.uid, fd);
    
    err = handle_connection(fd);
    if (err < 0) {
        climpd_log_e(tag, "handling connection on socket %d failed - %s\n",
                     fd, strerr(-err));
        goto out;
    }
    
out:
    close(fd);
    
    climpd_log_i(tag, "served connection on socket %d in %lu ms\n", fd, 
                 clock_elapsed_ms(&timer));
    
    return true;
}

static void handle_error_signal(int sig)
{
#define STACKTRACE_BUFFER_SIZE 32
    static void *buffer[STACKTRACE_BUFFER_SIZE];
    int size;
    
    climpd_log_e(tag, "received signal \"%s\"\nbacktrace:\n", strsignal(sig));
    
    size = backtrace(buffer, STACKTRACE_BUFFER_SIZE);
    if(size <= 0)
        return;
    
    backtrace_symbols_fd(buffer, size, climpd_log_fd());
    
    exit(EXIT_FAILURE);
}

static void handle_signal(int sig)
{
    const char *str;
    
    str = strsignal(sig);
    
    switch(sig) {
    case SIGTERM:
        climpd_log_i(tag, "terminating process on signal \"%s\"\n", str);
        g_main_loop_quit(main_loop);
        break;
    case SIGHUP:
        climpd_log_i(tag, "reloading config on signal \"%s\"\n", str);
        climpd_config_reload(conf);
        climpd_player_set_config(&player, conf);
        break;
    default:
        climpd_log_i(tag, "ignoring signal \"%s\"\n", str);
        break;
    }
}

static int close_std_streams(void)
{
    int streams[] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };
    int fd, err;
    
    fd = open("/dev/null", O_RDWR);
    if(fd < 0)
        return -errno;
    
    for(unsigned int i = 0; i < ARRAY_SIZE(streams); ++i) {
        err = dup2(fd, streams[i]);
        if(err < 0) {
            close(fd);
            return -errno;
        }
    }
    
    close(fd);
    
    return 0;
}

static int setup_signal_handlers(void)
{
    struct sigaction sa;
    static const int error_signals[] = { 
        SIGILL, SIGBUS, SIGSEGV, SIGFPE, SIGPIPE, SIGSYS 
    };
    static const int signals[] = { 
        SIGQUIT, SIGTERM, SIGHUP, SIGINT, SIGTSTP, SIGTTIN, SIGTTOU 
    };
    int err;
    
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = &handle_error_signal;
    sigemptyset(&sa.sa_mask);
    
    for(unsigned int i = 0; i < ARRAY_SIZE(error_signals); ++i) {
        err = sigaction(error_signals[i], &sa, NULL);
        if(err < 0)
            return -errno;
    }
    
    sa.sa_handler = &handle_signal;
    sigfillset(&sa.sa_mask);
    
    for(unsigned int i = 0; i < ARRAY_SIZE(signals); ++i) {
        err = sigaction(signals[i], &sa, NULL);
        if(err < 0)
            return -errno;
    }
    
    return 0;
}

static int daemonize(void)
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
    
    err = setup_signal_handlers();
    if(err < 0)
        return err;
    
    err = close_std_streams();
    if(err < 0)
        return err;
    
    return 0;
}

static int init_server_fd(void)
{
    struct sockaddr_un addr;
    int fd, err;
    
    /* Setup Unix Domain Socket */
    err = unlink(IPC_SOCKET_PATH);
    if(err < 0 && errno != ENOENT) {
        err = -errno;
        goto out;
    }
    
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) {
        err = -errno;
        goto out;
    }
    
    memset(&addr, 0, sizeof(addr));
    
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path));
    
    err = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    if(err < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    err = listen(fd, 5);
    if(err < 0) {
        err = -errno;
        goto cleanup1;
    }
    
    io_server = g_io_channel_unix_new(fd);
    if (!io_server) {
        err = -EIO;
        goto cleanup1;
    }
    
    g_io_add_watch(io_server, G_IO_IN, &handle_server_fd, NULL);
    g_io_channel_set_close_on_unref(io_server, true);

    return 0;

cleanup1:
    close(fd);
out:
    return err;
}

static void destroy_server_fd(void)
{
    g_io_channel_unref(io_server);
    unlink(IPC_SOCKET_PATH);
}

static int init(void)
{
    const struct map_config m_conf = {
        .size           = ARRAY_SIZE(options) << 2,
        .lower_bound    = MAP_DEFAULT_LOWER_BOUND,
        .upper_bound    = MAP_DEFAULT_UPPER_BOUND,
        .static_size    = false,
        .key_compare    = &compare_string,
        .key_hash       = &hash_string,
        .data_delete    = NULL,
    };
    int err;

    err = climpd_log_init();
    if(err < 0)
        return err;
    
    climpd_log_i(tag, "starting initialization...\n");
    
    if (!no_daemon) {
        err = daemonize();
        if(err < 0) {
            climpd_log_e(tag, "daemonize() - %s\n", strerr(-err));
            goto cleanup1;
        }
    }
        
    err = terminal_color_map_init();
    if(err < 0) {
        climpd_log_e(tag, "terminal_color_map_init() - %s\n", strerr(-err));
        goto cleanup1;
    }
    
    err = bool_map_init();
    if(err < 0) {
        climpd_log_e(tag, "bool_map_init() - %s\n", strerr(-err));
        goto cleanup2;
    }
    
    err = init_server_fd();
    if(err < 0) {
        climpd_log_e(tag, "init_server_fd(): %s\n", strerr(-err));
        goto cleanup3;
    }
    
    conf = climpd_config_new(".config/climp/climpd.conf");
    if (!conf) {
        err = -errno;
        goto cleanup4;
    }
    
    err = climpd_player_init(&player, conf);
    if (err < 0)
        goto cleanup5;
    
    err = media_discoverer_init(&disc);
    if (err < 0)
        goto cleanup6;
    
    err = clock_init(&timer, CLOCK_MONOTONIC);
    if (err < 0)
        goto cleanup7;
    
    clock_start(&timer);
    
    main_loop = g_main_loop_new(NULL, false);
    if (!main_loop) {
        err = -ENOMEM;
        goto cleanup8;
    }
    
    err = map_init(&options_map, &m_conf);
    if (err < 0)
        goto cleanup9;
    
    for (unsigned int i = 0; i < ARRAY_SIZE(options); ++i) {
        if (*options[i].l_opt != '\0') {
            err = map_insert(&options_map, options[i].l_opt, options + i);
            if (err < 0)
                goto cleanup10;
        }
        
        if (*options[i].s_opt != '\0') {
            err = map_insert(&options_map, options[i].s_opt, options + i);
            if (err < 0)
                goto cleanup10;
        }
    }
    
    climpd_log_i(tag, "initialization successful\n");
    
    return 0;

cleanup10:
    map_destroy(&options_map);
cleanup9:
    g_main_loop_unref(main_loop);
cleanup8:
    clock_destroy(&timer);
cleanup7:
    media_discoverer_destroy(&disc);
cleanup6:
    climpd_player_destroy(&player);
cleanup5:
    climpd_config_delete(conf);
cleanup4:
    destroy_server_fd();
cleanup3:
    bool_map_destroy();
cleanup2:
    terminal_color_map_destroy();
cleanup1:

    climpd_log_i(tag, "initialization failed\n");
    climpd_log_destroy();
    
    return err;
}

static void destroy(void)
{
    map_destroy(&options_map);
    g_main_loop_unref(main_loop);
    clock_destroy(&timer);
    media_discoverer_destroy(&disc);
    climpd_player_destroy(&player);
    climpd_config_delete(conf);
    destroy_server_fd();
    
    bool_map_destroy();
    terminal_color_map_destroy();
     
    climpd_log_i(tag, "destroyed\n");
    climpd_log_destroy();
}

int main(int argc, char *argv[])
{
    int err;
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp("--no-daemon", argv[i]) == 0 || strcmp("-n", argv[i]) == 0)
            no_daemon = true;
    }
    
    if(getuid() == 0)
        exit(EXIT_FAILURE);

    setlocale(LC_ALL, "");
    
    gst_init(NULL, NULL);
    
    err = init();
    if(err < 0)
        exit(EXIT_FAILURE);

    g_main_loop_run(main_loop);

    destroy();
    
    gst_deinit();
    
    return EXIT_SUCCESS;
}