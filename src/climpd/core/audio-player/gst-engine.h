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

#ifndef _GST_ENGINE_H_
#define _GST_ENGINE_H_

#include <stdbool.h>

#include <gst/gst.h>

enum gst_engine_state {
    GST_ENGINE_PLAYING = GST_STATE_PLAYING,
    GST_ENGINE_PAUSED  = GST_STATE_PAUSED,
    GST_ENGINE_STOPPED = GST_STATE_NULL,
};

struct gst_engine;

typedef void (*eos_callback)(struct gst_engine *);
typedef void (*bus_error_callback)(struct gst_engine *, 
                                   const char *, 
                                   const char *, 
                                   const char *);

struct gst_engine {
    GstElement *gst_pipeline;
    GstElement *gst_source;
    GstElement *gst_convert;
    GstElement *gst_pitch;
    GstElement *gst_volume;
    GstElement *gst_sink;
    GstState gst_state;
    
    unsigned int volume;
    bool mute;

    eos_callback on_end_of_stream;
    bus_error_callback on_bus_error;
};

int gst_engine_init(struct gst_engine *__restrict en);

void gst_engine_destroy(struct gst_engine *__restrict en);

void gst_engine_set_uri(struct gst_engine *__restrict en, 
                        const char *__restrict uri);

int gst_engine_play(struct gst_engine *__restrict en);

int gst_engine_pause(struct gst_engine *__restrict en);

int gst_engine_stop(struct gst_engine *__restrict en);

int gst_engine_stream_position(const struct gst_engine *__restrict en);

int gst_engine_set_stream_position(struct gst_engine *__restrict en, 
                                   unsigned int sec);

void gst_engine_set_pitch(struct gst_engine *__restrict en, float pitch);

float gst_engine_pitch(const struct gst_engine *__restrict en);

void gst_engine_set_speed(struct gst_engine *__restrict en, float speed);

float gst_engine_speed(const struct gst_engine *__restrict en);

void gst_engine_set_volume(struct gst_engine *__restrict en, unsigned int vol);

unsigned int gst_engine_volume(const struct gst_engine *__restrict en);

void gst_engine_set_mute(struct gst_engine *__restrict en, bool mute);

bool gst_engine_is_muted(const struct gst_engine *__restrict en);

enum gst_engine_state gst_engine_state(const struct gst_engine *__restrict en);

void gst_engine_set_end_of_stream_handler(struct gst_engine *__restrict en, 
                                          eos_callback func);

void gst_engine_set_bus_error_handler(struct gst_engine *__restrict en, 
                                      bus_error_callback func);

#endif /* _GST_ENGINE_H_ */