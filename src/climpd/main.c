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
#include <stdarg.h>
#include <assert.h>
#include <execinfo.h>

#include <libvci/map.h>
#include <libvci/hash.h>
#include <libvci/compare.h>
#include <libvci/clock.h>
#include <libvci/macro.h>
#include <libvci/error.h>
#include <libvci/filesystem.h>

#include "../shared/ipc.h"

#include <core/climpd-log.h>
#include <core/audio-player/audio-player.h>
#include <core/climpd-config.h>
#include <core/daemonize.h>
#include <core/media-loader.h>
#include <core/climpd-config.h>
#include <core/argument-parser.h>

#include <ipc/socket-server.h>

#include <util/strconvert.h>

static const char *tag = "main";

/* client side standard fds */
static int fd_in;
static int fd_out;
static int fd_err;

static char *conf_path;
static char *playlist_path;
static char *loader_path;
static char *socket_path;

static struct audio_player audio_player;
static struct media_loader media_loader;
static struct climpd_config config;
static struct socket_server socket_server;
static struct argument_parser arg_parser;
static GMainLoop *main_loop;

static const char help[] = {
    "Usage:\n"
    "climp --cmd1 [[arg1] ...] --cmd2 [[arg1] ...]\n\n"
    "  -a, --add [args]       Add a .m3u/.txt and / or media file\n"
    "                         to the playlist\n"
    "      --clear            Clear the current playlist.\n"
    "      --config           Print the climpd configuration.\n"
    "      --remove [args]    Remove media files from the playlist.\n"
    "                         .m3u / .txt - or media files are accepted\n"
    "                         arguments.\n"
    "      --pause            Pause / unpause the player. This has no effect \n"
    "                         if the player is stopped.\n" 
    "      --playlist [args]  Print or set the current playlist. Pass\n"
    "                         media files and / or .m3u / .txt - files.\n"
    "      --repeat           Toggle repeat playlist.\n"
    "      --shuffle          Toggle shuffle.\n"
    "  -v, --volume [arg]     Set or get the volume of the climpd-player.\n"
    "  -q, --quit             Quit the climpd application.\n"
    "      --help             Print this help message\n"
    "      --previous         not implemented yet\n"
    "      --next             Play the next track in the playlist.\n"
    "  -p, --play [args]      Start playback, or set a playlist and start\n"
    "                         playback immediatley, or jump to a track in the\n"
    "                         playlist. Possible arguments are media files,\n"
    "                         .m3u / .txt files or numbers.\n"
    "      --files            Print all files in the current playlist\n"
    "      --mute             Mute or unmute the player\n"
    "      --seek [arg]       Get current position or jump to a position \n"
    "                         in the current track.\n"
    "                         Accepted time formats: [m:ss] - or just - [s]\n"
    "      --sort             Sort the playlist. /some/file01 will be before\n"
    "                         /some/file02 and so on. Useful if you forgot to\n"
    "                         sort the file in bash (use: sort -V).\n"
    "  -i, --stdin            Read playlist from stdin.\n"
    "      --stop             Stop the playback\n"
    "      --uris             Print for each file in the playlist the\n"
    "                         corresponding URI\n"
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

static void report_invalid_integer(const char *__restrict s, int err)
{
    eprint("climpd: unrecognized integer value '%s' - %s\n", s, strerr(err));
}

static void report_invalid_float(const char *__restrict s, int err)
{
    eprint("climpd: unrecognized float value '%s' - %s\n", s, strerr(err));
}

static void report_missing_arg(const char *__restrict cmd)
{
    eprint("climpd: \"%s\" is missing at least one argument\n", cmd);
}

static void report_arg_error(const char *__restrict cmd, 
                             const char *__restrict arg, 
                             int err)
{
    eprint("climpd: %s: \"%s\" - %s\n", cmd, arg, strerr(abs(err)));
}

static void report_error(const char *__restrict cmd, 
                         const char *__restrict msg,
                         int err)
{
    eprint("climpd: %s: %s - %s\n", cmd, msg, strerr(abs(err)));
}

static void report_invalid_time_format(const char *__restrict cmd, 
                                       const char *__restrict arg)
{
    eprint("climpd: %s: \"%s\" - invalid time format, "
           "use m:ss, m.ss, m,ss or just s\n", cmd, arg);
}

static void report_redundant_if_applicable(const char **argv, int argc)
{
    if (argc <= 0)
        return;
    
    eprint("climpd: ignoring redundant arguments - ");
    
    for (int i = 0; i < argc; ++i)
        eprint("%s ", argv[i]);
    
    eprint("\n");
}

static void report_invalid_arg(const char *arg)
{    
    eprint("ignoring invalid argument '%s'\n", arg);
}

// static int handle_add(const char **argv, int argc)
// {
//     static const char *cmd = "--add";
//     struct media_list ml;
//     int err;
//     
//     if (argc == 0) {
//         report_missing_arg(cmd);
//         return -EINVAL;
//     }
//     
//     err = media_list_init(&ml);
//     
//     for (int i = 0; i < argc; ++i) {
//         err = media_loader_load(argv[i], &ml);
//         if (err < 0) {
//             report_arg_error(cmd, argv[i], err);
//             goto cleanup1;
//         }
//     }
//     
//     err = climpd_player_add_media_list(&ml);
//     if (err < 0)
//         report_error(cmd, "failed to add items to playlist", err);
//     
// cleanup1:
//     media_list_destroy(&ml);
//     
//     return err;
// }
// 
// static int handle_clear(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     climpd_player_clear_playlist();
//     
//     return 0;
// }
// 
// static int handle_config(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv + 1, argc - 1);
//     
//     if (argc == 0) {
//         climpd_config_print(fd_out);
//         return 0;
//     }
//     
//     /* TODO: reload config from 'argv[0]' */
//     
//     return 0;
// }
// 
// int handle_current(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     climpd_player_print_current_track(fd_out);
//     
//     return 0;
// }
// 
// static int handle_files(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     climpd_player_print_files(fd_out);
//     
//     return 0;
// }
// 
// static int handle_help(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     print("%s\n", help);
//     
//     return 0;
// }
// 
// static int handle_playlist(const char **argv, int argc)
// {
//     static const char *cmd = "--playlist";
//     struct media_list ml;
//     int err;
//     
//     if (argc == 0) {
//         climpd_player_print_playlist(fd_out);
//         return 0;
//     }
//     
//     err = media_list_init(&ml);
//     if (err < 0) {
//         eprint("climpd: creating media list failed - %s\n", strerr(-err));
//         return err;
//     }
//     
//     for (int i = 0; i < argc; ++i) {
//         err = media_loader_load(argv[i], &ml);
//         if (err < 0) {
//             report_arg_error(cmd, argv[i], err);
//             goto cleanup1;
//         }
//     }
//     
//     err = climpd_player_set_media_list(&ml);
//     if (err < 0)
//         eprint("climpd: setting new media list failed - %s\n", strerr(-err));
//     
// cleanup1:
//     media_list_destroy(&ml);
//     
//     return err;
// }
// 
// static int handle_mute(const char **argv, int argc)
// {
//     bool muted;
//     
//     report_redundant_if_applicable(argv + 1, argc - 1);
//     
//     if (argc == 0) {
//         climpd_player_toggle_muted();        
//         return 0;
//     }
// 
//     if (str_to_bool(argv[0], &muted)) {
//         report_invalid_boolean(argv[0]);
//         return -EINVAL;
//     }
//     
//     climpd_player_set_muted(muted);
//     
//     return 0;
// }
// 
// static int handle_next(const char **argv, int argc)
// {
//     static const char *cmd = "--next";
//     int err;
//     
//     report_redundant_if_applicable(argv, argc);
//     
//     err = climpd_player_next();
//     if (err < 0) {
//         report_error(cmd, "failed to play next track", err);
//         return err;
//     }
//     
//     if (climpd_player_is_stopped())
//         print("climpd: %s: finished playback\n", cmd);
//     
//     return 0;
// }
// 
// static int handle_pause(const char **argv, int argc)
// {
//     int err;
//     
//     report_redundant_if_applicable(argv, argc);
//     
//     switch (climpd_player_state()) {
//     case CLIMPD_PLAYER_PLAYING:
//         climpd_player_pause();
//         break;
//     case CLIMPD_PLAYER_PAUSED:
//         err = climpd_player_play();
//         if (err < 0) {
//             report_error("--pause", "unable to resume playing", err);
//             return err;
//         }
//         break;
//     case CLIMPD_PLAYER_STOPPED:
//         print("climpd: --pause: player is stopped - done\n");
//         break;
//     default:
//         break;
//     }
//     
//     return 0;
// }
// 
// static int handle_play(const char **argv, int argc)
// {
//     static const char *cmd = "--play";
//     struct media_list ml;
//     int index;
//     bool valid_index;
//     int err;
//     
//     if (argc == 0)
//         return climpd_player_play();
//     
//     err = media_list_init(&ml);
//     if (err < 0) {
//         report_arg_error(cmd, argv[0], err);
//         return err;
//     }
//     
//     valid_index = false;
//     
//     for (int i = 0; i < argc; ++i) {
//         err = str_to_int(argv[i], &index);
//         if (err == 0) {
//             valid_index = true;
//             continue;
//         }
//         
//         err = media_loader_load(argv[i], &ml);
//         if (err == 0)
//             continue;
//         
//         report_arg_error(cmd, argv[i], err);
//         goto cleanup1;
//     }
//     
//     /* 
//      * Run this first so indices become valid especially usefull 
//      * if loading playlist and jumping directly  to a track, e.g.:
//      * 
//      *     climp --clear --play my_playlist.m3u 10
//      */
//     if (!media_list_empty(&ml)) {
//         err = climpd_player_set_media_list(&ml);
//         if (err < 0) {
//             report_error(cmd, "adding files to player failed", err);
//             goto cleanup1;
//         }
//         
//         if (!valid_index) {
//             err = climpd_player_next();
//             if (err < 0)
//                 report_error(cmd, "failed to play track", err);
//         }
//     }
//     
//     if (valid_index) {
//         err = climpd_player_play_track(index);
//         if (err < 0) {
//             eprint("climpd: %s: failed to play track \"%d\" - %s\n", cmd, index,
//                    strerr(-err));
//         }
//     }
//     
// cleanup1:
//     media_list_destroy(&ml);
//     
//     return err;
// }
// 
// static int handle_prev(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     assert(0 && "NOT IMPLEMENTED");
//     
//     return 0;
// }
// 
// static int handle_quit(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     mainloop_quit();
//     
//     return 0;
// }
// 
// static int handle_remove(const char **argv, int argc)
// {
//     static const char *cmd = "--remove";
//     struct media_list ml;
//     int err;
//     
//     err = media_list_init(&ml);
//     
//     for (int i = 0; i < argc; ++i) {
//         int index;
//         
//         err = str_to_int(argv[i], &index);
//         if (err == 0) {
//             climpd_player_delete_index(index);
//             continue;
//         }
//         
//         err = media_loader_load(argv[i], &ml);
//         if (err < 0) {
//             report_arg_error(cmd, argv[i], err);
//             goto cleanup1;
//         }
//     }
//     
//     climpd_player_remove_media_list(&ml);
//     
// cleanup1:
//     media_list_destroy(&ml);
//     
//     return 0;
// }
// 
// static int handle_repeat(const char **argv, int argc)
// {
//     bool repeat;
//     
//     report_redundant_if_applicable(argv + 1, argc - 1);
//     
//     if (argc == 0) {
//         bool val = climpd_player_toggle_repeat();
//         
//         dprintf(fd_out, "  Repeating is now %s\n", (val) ? "ON" : "OFF");
//         return 0;
//     }
//     
//     if (str_to_bool(argv[0], &repeat) < 0) {
//         report_invalid_boolean(argv[0]);
//         return -EINVAL;
//     }
//     
//     climpd_player_set_repeat(repeat);
//     
//     return 0;
// }
// 
// static int handle_seek(const char **argv, int argc)
// {
//     int sec, err;
//     
//     report_redundant_if_applicable(argv + 1, argc - 1);
//     
//     if (argc == 0) {
//         int val = climpd_player_peek();
//         
//         int min = val / 60;
//         int sec = val % 60;
//         
//         print("  %d:%02d\n", min, sec);
//         return 0;
//     }
//     
//     err = str_to_sec(argv[0], &sec);
//     if (err < 0) {
//         report_invalid_time_format("--seek", argv[0]);
//         return err;
//     }
//     
//     err = climpd_player_seek((unsigned int) sec);
//     if(err < 0) {
//         report_error("--seek", "seeking to position failed", err);
//         return err;
//     }
//     
//     return 0;
// }
// 
// static int handle_shuffle(const char **argv, int argc)
// {
//     bool shuffle;
//     
//     report_redundant_if_applicable(argv + 1, argc - 1);
//     
//     if (argc == 0) {
//         bool shuffle = climpd_player_toggle_shuffle();        
//         
//         dprintf(fd_out, "  Shuffling is now %s\n", (shuffle) ? "ON" : "OFF");
//         return 0;
//     } 
// 
//     if (str_to_bool(argv[0], &shuffle) < 0) {
//         report_invalid_boolean(argv[0]);
//         return -EINVAL;
//     }
//     
//     climpd_player_set_shuffle(shuffle);
//     
//     return 0;
// }
// 
// static int handle_sort(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     climpd_player_sort_playlist();
//     
//     return 0;
// }
// 
// static int handle_stdin(const char **argv, int argc)
// {
//     static const char *cmd = "--stdin";
//     struct media_list ml;
//     int err;
//     
//     report_redundant_if_applicable(argv, argc);
//     
//     if (isatty(fd_in)) {
//         err = -EPIPE;
//         report_error(cmd, "stdin is attached to a terminal", err);
//         return err;
//     }
//     
//     err = media_list_init(&ml);
//     if (err < 0) {
//         report_error(cmd, "failed to initialize media-list", err);
//         return err;
//     }
//     
//     err = media_list_add_from_fd(&ml, fd_in);
//     if (err < 0) {
//         report_error(cmd, "failed to load files from stdin", err);
//         goto cleanup1;
//     }
//     
//     if (!media_list_empty(&ml)) {
//         err = climpd_player_set_media_list(&ml);
//         if (err < 0) {
//             report_error(cmd, "failed to set player media-list", err);
//             goto cleanup1;
//         }
//     } else {
//         report_error(cmd, "no (valid) data to read from stdin", -ENODATA);
//     }
//     
// cleanup1:
//     media_list_destroy(&ml);
//     
//     return err;
// }
// 
// static int handle_stop(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     climpd_player_stop();
//     
//     return 0;
// }
// 
// int handle_uris(const char **argv, int argc)
// {
//     report_redundant_if_applicable(argv, argc);
//     
//     climpd_player_print_uris(fd_out);
//     
//     return 0;
// }
// 
// 
// static int handle_volume(const char **argv, int argc)
// {
//     int vol, err;
//     
//     report_redundant_if_applicable(argv + 1, argc - 1);
//     
//     if (argc == 0) {
//         climpd_player_print_volume(fd_out);
//         return 0;
//     }
//     
//     err = str_to_int(argv[0], &vol);
//     if (err < 0) {
//         report_invalid_integer(argv[0]);
//         return -EINVAL;
//     }
//     
//     climpd_player_set_volume((unsigned int) abs(vol));
//     
//     return 0;
// }
// 
// static struct option options[] = {
//     { "--add",          "-a",   &handle_add             },
//     { "--clear",        "",     &handle_clear           },
//     { "--config",       "",     &handle_config          },
//     { "--current",      "-c",   &handle_current         },
//     { "--remove",       "",     &handle_remove          },
//     { "--pause",        "",     &handle_pause           },
//     { "--playlist",     "",     &handle_playlist        },
//     { "--repeat",       "",     &handle_repeat          },
//     { "--shuffle",      "",     &handle_shuffle         },
//     { "--volume",       "-v",   &handle_volume          },
//     { "--quit",         "-q",   &handle_quit            },
//     { "--help",         "",     &handle_help            },
//     { "--previous",     "",     &handle_prev            },
//     { "--next",         "-n",   &handle_next            },
//     { "--play",         "-p",   &handle_play            },
//     { "--files",        "",     &handle_files           },
//     { "--mute",         "-m",   &handle_mute            },
//     { "--seek",         "",     &handle_seek            },
//     { "--sort",         "",     &handle_sort            },
//     { "--stdin",        "-i",   &handle_stdin           },
//     { "--stop",         "",     &handle_stop            },
//     { "--uris",         "",     &handle_uris            },
// };

static int handle_add(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_clear(const char *cmd, const char **argv, int argc)
{
    struct playlist *playlist;
    (void) cmd;
    report_redundant_if_applicable(argv, argc);
    
    playlist = audio_player_playlist(&audio_player);
    playlist_clear(playlist);
    
    return 0;
}

static int handle_config(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_current(const char *cmd, const char **argv, int argc)
{
    struct playlist *playlist;
    struct media *m;
    struct media_info *info;
    unsigned int p_min, p_sec, index, meta_len;
    int position;
    
    (void) cmd;
    report_redundant_if_applicable(argv, argc);

    playlist = audio_player_playlist(&audio_player);
    index = playlist_index(playlist);
    if (index == (unsigned int) -1) {
        print("climpd: no current track\n");
        return 0;
    }
    
    p_min = 0;
    p_sec = 0;
    
    position = audio_player_stream_position(&audio_player);
    if (position != -1) {
        p_min = position / 60;
        p_sec = position % 60;
    }
    
    m = playlist_at_unsafe(playlist, index);
    info = media_info(m);

    meta_len = climpd_config_console_output_config(&config)->meta_column_width;
    
    eprint(" ( %3u )  %2u:%02u / %2u:%02u   %-*.*s %-*.*s %-*.*s\n",
           index, p_min, p_sec, info->duration / 60, info->duration % 60, 
           meta_len, meta_len, info->title,
           meta_len, meta_len, info->artist,
           meta_len, meta_len, info->album);
    
    return 0;
}

static int handle_files(const char *cmd, const char **argv, int argc)
{
    struct playlist *playlist;
    unsigned int size;

    (void) cmd;
    
    report_redundant_if_applicable(argv, argc);

    playlist = audio_player_playlist(&audio_player);
    
    size = playlist_size(playlist);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = playlist_at_unsafe(playlist, (int) i);
        
        eprint("%s\n", media_path(m));
    }
    
    return 0;
}

static int handle_help(const char *cmd, const char **argv, int argc)
{
    (void) cmd;
    (void) argv;
    (void) argc;
    
    print("%s\n", help);
    
    return 0;
}

static int handle_mute(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_next(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_pause(const char *cmd, const char **argv, int argc)
{
    int err;
    
    report_redundant_if_applicable(argv, argc);
    
    if (audio_player_is_playing(&audio_player)) {
        err = audio_player_pause(&audio_player);
        if (err < 0)
            report_error(cmd, "failed to pause playback", err);
    } else if (audio_player_is_paused(&audio_player)) {
        err = audio_player_play(&audio_player);
        if (err < 0)
            report_error(cmd, "failed to unpause playback", err);
    } else {
        err = -EINVAL;
        report_error(cmd, "unable to pause while stopped", err);
    }
    
    return err;
}

static int handle_play(const char *cmd, const char **argv, int argc)
{
    struct playlist *playlist;
    int cnt, index, err;
    
    if (argc == 0) {
        err = audio_player_play(&audio_player);
        return err;
    }
    
    playlist = audio_player_playlist(&audio_player);    
    cnt = 0;
    
    /* get number of valid indices */
    for (int i = 0; i < argc; ++i)
        cnt += str_is_int(argv[i]) == true;
    
    /* if there are more args then valid indices -> new media files */
    if (cnt != argc)
        playlist_clear(playlist);
    
    for (int i = 0; i < argc; ++i) {
        if (str_is_int(argv[i])) {
            err = str_to_int(argv[i], &index);
            if (err < 0) {
                report_invalid_integer(argv[i], err);
                goto fail;
            }
        } else {
            err = media_loader_load(&media_loader, argv[i], playlist);
            if (err < 0) {
                report_arg_error(cmd, argv[i], err);
                goto fail;
            }
        }
    }

    if (cnt != 0) {
        err = audio_player_play_track(&audio_player, index);
        if (err < 0) {
            report_error(cmd, "failed to play track", err);
            return err;
        }
    } else {
        err = audio_player_play_next(&audio_player);
        if (err < 0) {
            report_error(cmd, "failed to start playback", err);
            return err;
        }
    }
    
    err = playlist_save(playlist, playlist_path);
    if (err < 0)
        climpd_log_w(tag, "failed to save new playlist\n");
    
    return 0;

fail:
    playlist_clear(playlist);
    return err;
}

static int handle_playlist(const char *cmd, const char **argv, int argc)
{
    struct playlist *playlist;
    int err;
    
    playlist = audio_player_playlist(&audio_player);
    
    if (argc == 0) {
        unsigned int size;
        struct console_output_config *cout_conf;
        unsigned int meta_len;
        
        size = playlist_size(playlist);
        
        cout_conf = climpd_config_console_output_config(&config);
        meta_len = cout_conf->meta_column_width;
        
        for (unsigned int i = 0; i < size; ++i) {
            struct media *m;
            struct media_info *info;
            unsigned int min, sec;
            
            m = playlist_at_unsafe(playlist, (int) i);
            info = media_info(m);
            
            min = info->duration / 60;
            sec = info->duration % 60;
            
            print(" ( %3u )    %2u:%02u   %-*.*s %-*.*s %-*.*s\n",
                  i, min, sec, 
                  meta_len, meta_len, info->title,
                  meta_len, meta_len, info->artist,
                  meta_len, meta_len, info->album);
        }
        
        return 0;
    }
    
    playlist_clear(playlist);
    
    for (int i = 0; i < argc; ++i) {
        err = media_loader_load(&media_loader, argv[i], playlist);
        if (err < 0) {
            report_error(cmd, "failed to load media", err);
            return err;
        }
    }
    
    return 0;
}

static int handle_pitch(const char *cmd, const char **argv, int argc)
{
    float pitch, new_pitch;
    int err;
    
    (void) cmd;
    
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    pitch = audio_player_pitch(&audio_player);
    if (argc == 0) {
        print("  pitch: %f\n", pitch);
        return 0;
    }
    
    err = str_to_float(argv[0], &new_pitch);
    if (err < 0) {
        report_invalid_float(argv[0], err);
        return -EINVAL;
    }
    
    audio_player_set_pitch(&audio_player, new_pitch);
    
    if (pitch != new_pitch && climpd_config_keep_changes(&config)) {
        struct audio_player_config *ap_conf;
        
        ap_conf = climpd_config_audio_player_config(&config);
        ap_conf->pitch = new_pitch;
        
        climpd_config_save(&config);
    }
        
    return 0;
}

static int handle_previous(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_quit(const char *cmd, const char **argv, int argc)
{
    (void) cmd;
    
    report_redundant_if_applicable(argv, argc);
    
    g_main_loop_quit(main_loop);
    
    return 0;
}

static int handle_remove(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_repeat(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_seek(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_shuffle(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_sort(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_speed(const char *cmd, const char **argv, int argc)
{
    float speed, new_speed;
    int err;
    
    (void) cmd;
        
    report_redundant_if_applicable(argv + 1, argc - 1);
    
    speed = audio_player_pitch(&audio_player);
    if (argc == 0) {
        print("  speed: %f\n", speed);
        return 0;
    }
    
    err = str_to_float(argv[0], &new_speed);
    if (err < 0) {
        report_invalid_float(argv[0], err);
        return -EINVAL;
    }
    
    audio_player_set_speed(&audio_player, new_speed);
    
    if (speed != new_speed && climpd_config_keep_changes(&config)) {
        struct audio_player_config *ap_conf;
            
        ap_conf = climpd_config_audio_player_config(&config);
        ap_conf->speed = new_speed;
        
        climpd_config_save(&config);
    }
    
    return 0;
}

static int handle_stdin(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static int handle_stop(const char *cmd, const char **argv, int argc)
{
    int err;
    
    (void) argv;
    (void) argc;
    
    err = audio_player_stop(&audio_player);
    if (err < 0)
        report_error(cmd, "failed to stop playback", err);
    
    return err;
}

static int handle_uris(const char *cmd, const char **argv, int argc)
{
    struct playlist *playlist;
    unsigned int size;
    
    (void) cmd;
    report_redundant_if_applicable(argv, argc);
    
    playlist = audio_player_playlist(&audio_player);
    size = playlist_size(playlist);
    
    for (unsigned int i = 0; i < size; ++i) {
        struct media *m = playlist_at_unsafe(playlist, (int) i);
        
        eprint("%s\n", media_uri(m));
    }
    
    return 0;
}

static int handle_volume(const char *cmd, const char **argv, int argc)
{
    (void) argv;
    (void) argc;
    
    eprint("climpd: %s: not implemented\n", cmd);
    
    return 0;
}

static struct arg args[] = {
    { "--add",          "-a",   &handle_add             },
    { "--clear",        "",     &handle_clear           },
    { "--config",       "",     &handle_config          },
    { "--current",      "-c",   &handle_current         },
    { "--files",        "",     &handle_files           },
    { "--help",         "",     &handle_help            },
    { "--mute",         "-m",   &handle_mute            },
    { "--next",         "-n",   &handle_next            },
    { "--pause",        "",     &handle_pause           },
    { "--play",         "-p",   &handle_play            },
    { "--playlist",     "",     &handle_playlist        },
    { "--pitch",        "",     &handle_pitch           },
    { "--previous",     "",     &handle_previous        },
    { "--quit",         "-q",   &handle_quit            },
    { "--remove",       "",     &handle_remove          },
    { "--repeat",       "",     &handle_repeat          },
    { "--seek",         "",     &handle_seek            },
    { "--shuffle",      "",     &handle_shuffle         },
    { "--sort",         "",     &handle_sort            },
    { "--speed",        "",     &handle_speed           },
    { "--stdin",        "",     &handle_stdin           },
    { "--stop",         "",     &handle_stop            },
    { "--uris",         "",     &handle_uris            },
    { "--volume",       "-v",   &handle_volume          },
};

static int handle_connection(int fd)
{
    char **argv, *cwd;
    int argc, err;
    
    err = ipc_recv_setup(fd, &fd_in, &fd_out, &fd_err, &cwd);
    if (err < 0) {
        climpd_log_e(tag, "receiving client's fds failed - %s\n", strerr(-err));
        return err;
    }
    
    err = ipc_recv_argv(fd, &argv, &argc);
    if (err < 0) {
        climpd_log_e(tag, "receiving arguments failed - %s\n", strerr(-err));
        goto cleanup1;
    }
    
    /* necessary to handle relative paths */
    err = chdir(cwd);
    if (err < 0)
        climpd_log_w(tag, "chdir() to \"%s\" failed - %s\n", cwd, errstr);
    
    err = argument_parser_run(&arg_parser, (const char **) argv, argc);
    
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
    close(fd_in);
    
    return err;
}

void on_sighub(int signo, siginfo_t *info, void *context)
{
    (void) signo;
    (void) info;
    (void) context;
}

void on_sigterm(int signo, siginfo_t *info, void *context)
{
    (void) signo;
    (void) info;
    (void) context;
    
    g_main_loop_quit(main_loop);
}

__attribute__((noreturn))
void on_sigerr(int signo, siginfo_t *info, void *context)
{
    static void *buffer[32];
    int size;
    
    (void) info;
    (void) context;
    
    climpd_log_e(tag, "received signal \"%s\"\nbacktrace:\n", strsignal(signo));
    
    size = backtrace(buffer, ARRAY_SIZE(buffer));
    if(size > 0)
        backtrace_symbols_fd(buffer, size, climpd_log_fd());
    
    exit(EXIT_FAILURE);
}

struct signal_handle sighandles[] = {
    { SIGQUIT,          SIG_IGN     },
    { SIGINT,           SIG_IGN     },
    { SIGTSTP,          SIG_IGN     },
    { SIGTTIN,          SIG_IGN,    },
    { SIGTTOU,          SIG_IGN,    },
    { SIGPIPE,          SIG_IGN,    },
    { SIGHUP,           &on_sighub  },
    { SIGTERM,          &on_sigterm },
    { SIGILL,           &on_sigerr  },
    { SIGBUS,           &on_sigerr  },
    { SIGSEGV,          &on_sigerr  },
    { SIGFPE,           &on_sigerr  },
    { SIGSYS,           &on_sigerr  },
    
};

__attribute__((constructor)) void __init(void)
{
    char *path = NULL;
    int err;
    
    gst_init(NULL, NULL);
    
    err = asprintf(&path, "/tmp/climpd-%d.log", getuid());
    if (err < 0) {
        fprintf(stderr, "**FATAL: failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    
    err = climpd_log_init(path);
    
    free(path);
    
    if (err < 0) {
        fprintf(stderr, "**FATAL: failed to initialize log file - %s\n",
                strerr(-err));
        exit(EXIT_FAILURE);
    }
}

__attribute__((destructor)) void __destroy(void)
{
    climpd_log_destroy();
    gst_deinit();
}

__attribute__((noreturn)) void die_error(void)
{
    climpd_log_e(tag, "exiting due to previous error(s)\n");
    exit(EXIT_FAILURE);;
}

int main(int argc, char *argv[])
{
    struct audio_player_config *player_config;
    struct playlist *playlist;
    const char *home;
    bool no_daemon = false;
    int err;
    
    if(getuid() == 0)
        exit(EXIT_FAILURE);
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp("--no-daemon", argv[i]) == 0 || strcmp("-n", argv[i]) == 0)
            no_daemon = true;
    }

    climpd_log_i(tag, "starting initialization...\n");
    
    if (!no_daemon) {
        err = daemonize(sighandles, ARRAY_SIZE(sighandles));
        if (err < 0) {
            climpd_log_e(tag, "failed to daemonize process\n");
            die_error();
        }
    }
    
    home = getenv("HOME");
    if (!home) {
        climpd_log_e(tag, "failed to locate users home directory\n");
        die_error();
    }

    err = asprintf(&conf_path, "%s/.config/climp/climpd.conf", home);
    if (err < 0) {
        climpd_log_e(tag, "failed to locate the configuration file\n");
        die_error();
    }
    
    err = climpd_config_init(&config, conf_path);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize configuration file - %s\n",
                     strerr(-err));
        die_error();
    }

    player_config = climpd_config_audio_player_config(&config);
    
    err = audio_player_init(&audio_player);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize audio player - %s\n", 
                     strerr(-err));
        die_error();
    }
    
    err = asprintf(&playlist_path, "%s/.config/climp/playlists/__playlist.m3u", 
                   home);
    if (err < 0) {
        climpd_log_e(tag, "failed to locate path to last playlist\n");
        die_error();
    }
    
    audio_player_set_volume(&audio_player, player_config->volume);
    audio_player_set_pitch(&audio_player, player_config->pitch);
    audio_player_set_speed(&audio_player, player_config->speed);
    
    playlist = audio_player_playlist(&audio_player);
    
    if (path_exists(playlist_path)) {
        err = playlist_load(playlist, playlist_path);
        if (err < 0)
            climpd_log_w(tag, "failed to load last playlist - continuing\n");
    }
    
    playlist_set_repeat(playlist, player_config->repeat);
    playlist_set_shuffle(playlist, player_config->shuffle);
    
    err = asprintf(&loader_path, "%s/.config/climp/playlists/", home);
    if (err < 0) {
        climpd_log_e(tag, "failed to locate playlist folder\n");
        die_error();
    }
    
    err = media_loader_init(&media_loader);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize media loader - %s\n",
                     strerr(-err));
        die_error();
    }
    
    err = media_loader_add_dir(&media_loader, loader_path);
    if (err < 0) {
        climpd_log_e(tag, "failed to add search directory to media loader\n");
        die_error();
    }
    
    err = asprintf(&socket_path, "/tmp/.climpd-%d.sock", getuid());
    if (err < 0) {
        climpd_log_e(tag, "failed to create path to server socket\n");
        die_error();
    }
    
    err = argument_parser_init(&arg_parser, args, ARRAY_SIZE(args));
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize the argument handler - %s\n",
                     strerr(-err));
        die_error();
    }
    
    argument_parser_set_default_handler(&arg_parser, &report_invalid_arg);
    
    err = socket_server_init(&socket_server, socket_path, &handle_connection);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize server socket - %s\n", 
                     strerr(-err));
        die_error();
    }
    
    main_loop = g_main_loop_new(NULL, false);
    if (!main_loop) {
        climpd_log_e(tag, "failed to initialize main loop\n");
        die_error();
    }

    climpd_log_i(tag, "initialization successful\n");
    
    g_main_loop_run(main_loop);
    
    err = playlist_save(playlist, playlist_path);
    if (err < 0)
        climpd_log_w(tag, "failed to save playlist - continuing shutdown\n");
    
    free(loader_path);
    free(playlist_path);
    free(conf_path);
    
    g_main_loop_unref(main_loop);
    socket_server_destroy(&socket_server);
    argument_parser_destroy(&arg_parser);
    media_loader_destroy(&media_loader);
    audio_player_destroy(&audio_player);
    climpd_config_destroy(&config);
    
    return EXIT_SUCCESS;
}