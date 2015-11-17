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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <pwd.h>

#include <libvci/options.h>
#include <libvci/macro.h>
#include <libvci/error.h>

#include "../climpd/core/climpd-log.h"
#include "../climpd/core/audio-player/audio-player.h"
#include "../climpd/core/playlist/playlist.h"
#include "../climpd/util/strconvert.h"


struct program_option po[] = {
    PROGRAM_OPTION_INIT("--volume", "-v", 1),
    PROGRAM_OPTION_INIT("--speed", "-s", 1),
    PROGRAM_OPTION_INIT("--pitch", "", 1),
    PROGRAM_OPTION_INIT("--play", "-p", -1),
    PROGRAM_OPTION_INIT("--repeat", "-r", 0),
};

struct program_option *opt_vol       = po + 0;
struct program_option *opt_speed     = po + 1;
struct program_option *opt_pitch     = po + 2;
struct program_option *opt_play      = po + 3;
struct program_option *opt_repeat    = po + 4;

void die(const char *__restrict msg)
{
    fprintf(stderr, "**ERROR: %s\n", msg);
    exit(EXIT_FAILURE);
}

bool is_text_file(const char *path)
{
    return strstr(path, ".m3u") || strstr(path, ".txt");
}

int drop_privileges(void)
{
    struct passwd *passwd = getpwnam("nobody");
    if (!passwd)
        return -ENOTSUP;
    
    if (setuid(passwd->pw_uid) < 0)
        return -errno;
    
    return 0;
}

int main(int argc, char *argv[])
{
    struct audio_player player;
    struct playlist *pl;
    GMainLoop *loop;
    char *err_msg = NULL;
    int err;
    
    if (getuid() == 0) {
        err = drop_privileges();
        if (err < 0)
            die("failed to drop privileges - can't run as root");
    }
    
    gst_init(NULL, NULL);
    err = climpd_log_init("/tmp/audio_player.log");
    if (err < 0)
        die("failed to initialize log file");
    
    err = options_parse(po, ARRAY_SIZE(po), argv + 1, argc - 1, &err_msg);
    if (err < 0) {
        if (err_msg)
            die(err_msg);
        else
            die("failed to parse arguments");
    }
    
    err = audio_player_init(&player);
    if (err < 0)
        die("failed to initialize audio player");
    
    audio_player_set_volume(&player, 60);
    
    if (opt_vol->passed) {
        int vol;
        
        err = str_to_int(opt_vol->argv[0], &vol);
        if (err < 0)
            die("failed to read volume from arguments");
        
        audio_player_set_volume(&player, vol);
    }
    
    if (opt_speed->passed) {
        float speed;
        
        err = str_to_float(opt_speed->argv[0], &speed);
        if (err < 0)
            die("failed to read speed from arguments");
        
        audio_player_set_speed(&player, speed);
    }
    
    if (opt_pitch->passed) {
        float pitch;
        
        err = str_to_float(opt_speed->argv[0], &pitch);
        if (err < 0)
            die("failed to read pitch from arguments");
        
        audio_player_set_pitch(&player, pitch);
    }
    
    pl = audio_player_playlist(&player);
    
    if (opt_repeat->passed)
        playlist_set_repeat(pl, true);
    
    for (int i = 0; i < opt_play->argc; ++i) {
        if (is_text_file(opt_play->argv[i])) {
            err = playlist_load(pl, opt_play->argv[i]);
            if (err < 0) {
                fprintf(stderr, "**WARNING: failed to load '%s' - %s\n",
                        opt_play->argv[i], strerr(-err));
            }
        } else {
            err = playlist_add(pl, opt_play->argv[i]);
            if (err < 0) {
                fprintf(stderr, "**WARNING: failed to add '%s' - %s\n",
                        opt_play->argv[i], strerr(-err));
            }
        }
    }
    
    if (playlist_empty(pl)) {
        fprintf(stdout, "**WARNING: playlist is empty\n");
        exit(EXIT_SUCCESS);
    }
    
    err = audio_player_play(&player);
    if (err < 0)
        die("failed to play audio files");
    
    loop = g_main_loop_new(NULL, false);
    if (!loop)
        die("failed to initialize main loop");
    
    g_main_loop_run(loop);
    
    g_main_loop_unref(loop);
    audio_player_destroy(&player);
    gst_deinit();
    
    return 0;
}