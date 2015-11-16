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

#include <string.h>
#include <errno.h>

#include <libvci/macro.h>

#include <core/climpd-log.h>
#include <core/audio-player/audio-player.h>
#include <core/playlist/playlist.h>

static const char *tag = "audio-player";

static void handle_bus_error(struct gst_engine *en, 
                             const char *name, 
                             const char *msg, 
                             const char *info)
{
    struct audio_player *ap = container_of(en, struct audio_player, engine);
    
    climpd_log_e(tag, "bus error from '%s': %s\n", name, msg);
    
    if (info)
        climpd_log_e(tag, "additional debug info: %s\n", info);
    
    if (ap->active_track) {
        const char *uri = media_uri(ap->active_track);
     
        climpd_log_e(tag, "played '%s' when bus error occurred\n", uri);
    }
}

static void handle_end_of_stream(struct gst_engine *en)
{
    struct audio_player *ap = container_of(en, struct audio_player, engine);

    audio_player_play_next(ap);
}

int audio_player_init(struct audio_player *__restrict ap)
{
    int err;
    
    memset(ap, 0, sizeof(*ap));
    
    err = gst_engine_init(&ap->engine);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize engine\n");
        return err;
    }
    
    gst_engine_set_end_of_stream_handler(&ap->engine, &handle_end_of_stream);
    gst_engine_set_bus_error_handler(&ap->engine, &handle_bus_error);
    
    err = playlist_init(&ap->playlist);
    if (err < 0) {
        climpd_log_e(tag, "failed to initialize playlist\n");
        gst_engine_destroy(&ap->engine);
        return err;
    }
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;
}

void audio_player_destroy(struct audio_player *__restrict ap)
{
    if (ap->active_track)
        media_unref(ap->active_track);
    
    playlist_destroy(&ap->playlist);
    gst_engine_destroy(&ap->engine);
    
    climpd_log_i(tag, "destroyed\n");
}

int audio_player_play(struct audio_player *__restrict ap)
{
    int ret;
    
    switch (gst_engine_state(&ap->engine)) {
    case GST_ENGINE_STOPPED:
        ret = audio_player_play_next(ap);
        break;
    case GST_ENGINE_PAUSED:
        ret = gst_engine_play(&ap->engine);
        break;
    case GST_ENGINE_PLAYING:
    default:
        ret = 0;
        break;
    }
    
    return ret;
}

int audio_player_pause(struct audio_player *__restrict ap)
{
    return gst_engine_pause(&ap->engine);
}

int audio_player_stop(struct audio_player *__restrict ap)
{
    return gst_engine_stop(&ap->engine);
}

int audio_player_play_track(struct audio_player *__restrict ap, int track)
{
    struct media *m;
    int err;
    
    m = playlist_at(&ap->playlist, track);
    if (!m)
        return -EINVAL;
    
    err = gst_engine_stop(&ap->engine);
    if (err < 0) {
        climpd_log_e(tag, "failed to stop active track\n");
        goto fail;
    }
    
    gst_engine_set_uri(&ap->engine, media_uri(m));
    
    err = gst_engine_play(&ap->engine);
    if (err < 0) {
        climpd_log_e(tag, "failed to play newly set track\n");
        goto fail;
    }
    
    playlist_set_index(&ap->playlist, track);
    
    if (ap->active_track)
        media_unref(ap->active_track);
    
    ap->active_track = m;
    
    return 0;

fail:
    media_unref(m);
    return -ENOTSUP;
}

int audio_player_play_next(struct audio_player *__restrict ap)
{
    unsigned int track;
    
    if (playlist_empty(&ap->playlist)) {
        climpd_log_e(tag, "failed to play next track - playlist is empty\n");
        return -ENOMEDIUM;
    }
    
    track = playlist_next(&ap->playlist);
    if (track == (unsigned int) -1) {
        climpd_log_i(tag, "finished playlist\n");
        return 0;
    }
    
    return audio_player_play_track(ap, (int) track);
}

void audio_player_set_pitch(struct audio_player *__restrict ap, float pitch)
{
    gst_engine_set_pitch(&ap->engine, pitch);
}

float audio_player_pitch(const struct audio_player *__restrict ap)
{
    return gst_engine_pitch(&ap->engine);
}

void audio_player_set_speed(struct audio_player *__restrict ap, float speed)
{
    gst_engine_set_speed(&ap->engine, speed);
}

float audio_player_speed(const struct audio_player *__restrict ap)
{
    return gst_engine_speed(&ap->engine);
}

void audio_player_set_volume(struct audio_player *__restrict ap, 
                             unsigned int vol)
{
    gst_engine_set_volume(&ap->engine, vol);
}

unsigned int audio_player_volume(const struct audio_player *__restrict ap)
{
    return gst_engine_volume(&ap->engine);
}

void audio_player_set_mute(struct audio_player *__restrict ap, bool mute)
{
    gst_engine_set_mute(&ap->engine, mute);
}

bool audio_player_is_muted(const struct audio_player *__restrict ap)
{
    return gst_engine_is_muted(&ap->engine);
}

void audio_player_toggle_mute(struct audio_player *__restrict ap)
{
    bool mute = gst_engine_is_muted(&ap->engine);
    
    gst_engine_set_mute(&ap->engine, !mute);
}

bool audio_player_is_playing(const struct audio_player *__restrict ap)
{
    return gst_engine_state(&ap->engine) == GST_ENGINE_PLAYING;
}

bool audio_player_is_paused(const struct audio_player *__restrict ap)
{
    return gst_engine_state(&ap->engine) == GST_ENGINE_PAUSED;
}

bool audio_player_is_stopped(const struct audio_player *__restrict ap)
{
    return gst_engine_state(&ap->engine) == GST_ENGINE_STOPPED;
}

int audio_player_set_stream_position(struct audio_player *__restrict ap, 
                                     unsigned int sec)
{
    return gst_engine_set_stream_position(&ap->engine, sec);
}

int audio_player_stream_position(const struct audio_player *__restrict ap)
{
    return gst_engine_stream_position(&ap->engine);
}

struct playlist *audio_player_playlist(struct audio_player *__restrict ap)
{
    return &ap->playlist;
}

const struct media *
audio_player_active_track(const struct audio_player *__restrict ap)
{
    return ap->active_track;
}

