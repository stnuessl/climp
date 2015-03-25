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
#include <stdint.h>
#include <locale.h>
#include <stdarg.h>
#include <assert.h>

#include <libvci/map.h>
#include <libvci/hash.h>
#include <libvci/compare.h>
#include <libvci/clock.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include "../shared/ipc.h"
#include "../shared/constants.h"

#include <core/climpd-log.h>
#include <core/bool-map.h>
#include <core/terminal-color-map.h>
#include <core/climpd-paths.h>
#include <core/climpd-player.h>
#include <core/media-loader.h>
#include <core/media-discoverer.h>
#include <core/climpd-config.h>
#include <core/socket.h>
#include <core/mainloop.h>

#include <obj/media-list.h>

/*
 * TODO:        autoload last playlist
 */

static int handle_add(const char **argv, int argc);
static int handle_clear(const char **argv, int argc);
static int handle_config(const char **argv, int argc);
static int handle_current(const char **argv, int argc);
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
    { "--current",      "-c",   &handle_current         },
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

__attribute__((format(printf,1,2)))
static void print(const char *__restrict fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    vdprintf(fd_out, fmt, vargs);
    va_end(vargs);
}

__attribute__((format(printf,1,2)))
static void eprint(const char *__restrict fmt, ...)
{
    va_list vargs;
    
    va_start(vargs, fmt);
    vdprintf(fd_err, fmt, vargs);
    va_end(vargs);
}

static void report_invalid_boolean(const char *__restrict invalid_val)
{
    eprint("climpd: unrecognized boolean value '%s'\n", invalid_val);
}

static void report_invalid_integer(const char *__restrict invalid_val)
{
    eprint("climpd: unrecognized integer value '%s'\n", invalid_val);
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

// static int make_arg_insertion(struct media_list *__restrict ml, 
//                               const char *__restrict arg)
// {
//     struct stat st;
//     int err;
//     
//     err = stat(arg, &st);
//     if (err < 0)
//         return -errno;
//     
//     switch (st.st_mode & S_IFMT) {
//     case S_IFREG:
//         if (media_discoverer_file_is_playable(arg))
//             return media_list_emplace_back(ml, arg);
//         else 
//             return media_list_add_from_file(ml, arg);
//     case S_IFDIR:
//         return media_discoverer_scan_dir(arg, ml);
//     default:
//         return -EMEDIUMTYPE;
//     }
// }

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
        err = media_loader_load(argv[i], &ml);
        if (err < 0) {
            report_arg_error("--add", argv[i], err);
            goto cleanup1;
        }
    }
    
    /* TODO: finish function */
    
cleanup1:
    media_list_destroy(&ml);
    
    return err;
}

static int handle_clear(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    climpd_player_clear_playlist();
    
    return 0;
}

static int handle_config(const char **argv, int argc)
{
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        climpd_config_print(fd_out);
        return 0;
    }
    
    /* TODO: reload config from 'argv[0]' */
        
    return 0;
}

int handle_current(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);

    climpd_player_print_current_track(fd_out);
    
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
        err = media_discoverer_scan_all(argv[i], &ml);
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
    
    climpd_player_print_files(fd_out);
    
    return 0;
}

static int handle_help(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    print("%s\n", help);
    
    return 0;
}

static int handle_playlist(const char **argv, int argc)
{
    struct media_list ml;
    int err;
    
    if (argc == 0) {
        climpd_player_print_playlist(fd_out);
        return 0;
    }
    
    err = media_list_init(&ml);
    if (err < 0) {
        eprint("climpd: creating media list failed - %s\n", strerr(-err));
        return err;
    }
    
    for (int i = 0; i < argc; ++i) {
        err = media_loader_load(argv[i], &ml);
        if (err < 0) {
            report_arg_error("--playlist", argv[i], err);
            goto cleanup1;
        }
    }
    
    err = climpd_player_set_media_list(&ml);
    if (err < 0)
        eprint("climpd: setting new media list failed - %s\n", strerr(-err));
    
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
        climpd_player_toggle_muted();        
        return 0;
    }
    
    const bool *muted = bool_map_value(argv[0]);
    if (!muted) {
        report_invalid_boolean(argv[0]);
        return -EINVAL;
    }
    
    climpd_player_set_muted(*muted);
    
    return 0;
}

static int handle_next(const char **argv, int argc)
{
    int err;
    
    report_redundant_if_applicable(argv, argc);
    
    err = climpd_player_next();
    if (err < 0) {
        report_error("--next", "failed to play next track", err);
        return err;
    }
    
    if (climpd_player_is_stopped())
        print("climpd: --next: finished playback\n");
    
    return 0;
}

static int handle_pause(const char **argv, int argc)
{
    int err;
    
    report_redundant_if_applicable(argv, argc);

    switch (climpd_player_state()) {
    case CLIMPD_PLAYER_PLAYING:
        climpd_player_pause();
        break;
    case CLIMPD_PLAYER_PAUSED:
        err = climpd_player_play();
        if (err < 0) {
            report_error("--pause", "unable to resume playing", err);
            return err;
        }
        break;
    case CLIMPD_PLAYER_STOPPED:
        print("climpd: --pause: player is stopped - done\n");
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
        return climpd_player_play();
    
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
        
        err = media_loader_load(argv[i], &ml);
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
        err = climpd_player_add_media_list(&ml);
        if (err < 0) {
            report_error("--play", "adding files to player failed", err);
            goto cleanup1;
        }    
    }
    
    if (valid_index) {
        err = climpd_player_play_track(index);
        if (err < 0)
            report_error("--play", "failed to play specified index", err);
    } else if (!media_list_empty(&ml)) {
        struct media *m = media_list_at(&ml, 0);
        
        err = climpd_player_play_path(media_path(m));
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
    
    mainloop_quit();
    
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
            climpd_player_delete_index(index);
            continue;
        }
        
        err = media_loader_load(argv[i], &ml);
        if (err < 0) {
            report_arg_error("--remove", argv[i], err);
            goto cleanup1;
        }
    }
    
    climpd_player_remove_media_list(&ml);

cleanup1:
    media_list_destroy(&ml);
    
    return 0;
}

static int handle_repeat(const char **argv, int argc)
{
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        bool repeat = climpd_player_toggle_repeat();
        
        dprintf(fd_out, "  Repeating is now %s\n", (repeat) ? "ON" : "OFF");
        return 0;
    }
    
    const bool *repeat = bool_map_value(argv[0]);
    if (!repeat) {
        report_invalid_boolean(argv[0]);
        return -EINVAL;
    }
    
    climpd_player_set_repeat(*repeat);
    
    return 0;
}

static int handle_seek(const char **argv, int argc)
{
    char *r;
    long val;
    int err;
 
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        int val = climpd_player_peek();
        
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
    
    err = climpd_player_seek(val);
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
        bool shuffle = climpd_player_toggle_shuffle();        
        
        dprintf(fd_out, "  Shuffling is now %s\n", (shuffle) ? "ON" : "OFF");
        return 0;
    } 
    
    const bool *shuffle = bool_map_value(argv[0]);
    if (!shuffle) {
        report_invalid_boolean(argv[0]);
        return -EINVAL;
    }
    
    climpd_player_set_shuffle(*shuffle);
    
    return 0;
}

static int handle_stop(const char **argv, int argc)
{
    report_redundant_if_applicable(argv, argc);
    
    climpd_player_stop();
    
    return 0;
}

static int handle_volume(const char **argv, int argc)
{
    int vol, err;
    
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    if (argc == 0) {
        climpd_player_print_volume(fd_out);
        return 0;
    }
    
    err = string_to_integer(argv[0], &vol);
    if (err < 0) {
        report_invalid_integer(argv[0]);
        return -EINVAL;
    }

    climpd_player_set_volume((unsigned int) abs(vol));
    
    return 0;
}

static int handle_argv(const char **argv, int argc)
{
    int err;
    
    for (int i = 0 ; i < argc; ++i) {
        struct option *opt = map_retrieve(&options_map, argv[i]);
        
        if (!opt) {
            dprintf(fd_err, "climpd: invalid option \"%s\"\n", argv[i]);
            continue;
        }
        
        int j = i + 1;
        
        while (j < argc && !map_contains(&options_map, argv[j]))
            j++;
        
        j--;
        
        err = opt->handler(argv + i + 1, j - i);
        if (err < 0)
            climpd_log_w(tag, "\"%s\" failed - %s\n", argv[i], strerr(-err));
        
        i = j;
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
        climpd_log_w(tag, "chdir() to \"%s\" failed - %s\n", cwd, errstr);
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

static void options_init(void)
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
    
    err = map_init(&options_map, &m_conf);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize option map - %s\n", 
                     strerr(-err));
        goto fail;
    }
    
    for (unsigned int i = 0; i < ARRAY_SIZE(options); ++i) {
        if (*options[i].l_opt != '\0') {
            err = map_insert(&options_map, options[i].l_opt, options + i);
            if (err < 0) {
                climpd_log_e(tag, "failed to initialize option \"%s\" - %s\n", 
                             options[i].l_opt, strerr(-err));
                goto fail;
            }
        }
        
        if (*options[i].s_opt != '\0') {
            err = map_insert(&options_map, options[i].s_opt, options + i);
            if (err < 0) {
                climpd_log_e(tag, "failed to initialize option \"%s\" - %s\n", 
                             options[i].s_opt, strerr(-err));
                goto fail;
            }
        }
    }
    
    return;

fail:
    climpd_log_e(tag, "failed to initialize option handling - aborting...\n");
    exit(EXIT_FAILURE);
}

void options_destroy(void)
{
    map_destroy(&options_map);
}

int main(int argc, char *argv[])
{
    bool no_daemon = false;
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp("--no-daemon", argv[i]) == 0 || strcmp("-n", argv[i]) == 0)
            no_daemon = true;
    }
    
    if(getuid() == 0)
        exit(EXIT_FAILURE);

    setlocale(LC_ALL, "");
    
    gst_init(NULL, NULL);
    climpd_log_init();
    
    climpd_log_i(tag, "starting initialization...\n");

    terminal_color_map_init();
    bool_map_init();
    climpd_paths_init();
    mainloop_init(no_daemon);
    socket_init(&handle_connection);
    
    /* needs climpd_paths */
    climpd_config_init();
    media_discoverer_init();
    
    /* needs climpd_config */
    climpd_player_init();
    
    /* needs climpd_paths */
    media_loader_init();
    options_init();

    climpd_log_i(tag, "initialization successful\n");
    
    mainloop_run();
    
    options_destroy();
    media_loader_destroy();
    climpd_player_destroy();
    media_discoverer_destroy();
    climpd_config_destroy();
    socket_destroy();
    mainloop_destroy();
    climpd_paths_destroy();
    bool_map_destroy();
    terminal_color_map_destroy();
    
    climpd_log_i(tag, "destroyed\n");
    climpd_log_destroy();
    
    gst_deinit();
    
    return EXIT_SUCCESS;
}