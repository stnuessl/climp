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

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include <gst/gst.h>

#include <libvci/macro.h>

#include "media_player/media_player.h"
#include "media_player/playlist.h"
#include "media_player/media.h"

#include "config.h"
#include "color.h"
#include "client.h"

extern int media_meta_length;
extern const char *current_media_meta_color;
extern const char *media_meta_color;

extern struct climpd_config conf;

static void media_player_print_media(const struct client *__restrict client, 
                                     const struct media *m,
                                     int index,
                                     const char *color)
{
    const struct media_info *i;
    
    assert(m && "No track to print");
    
    i = media_info(m);

    client_out(client, 
               "%s    ( %3d )\t%-*.*s %-*.*s %-*.*s\n" COLOR_DEFAULT,
               color, index,
               conf.media_meta_length, conf.media_meta_length, i->title, 
               conf.media_meta_length, conf.media_meta_length, i->artist, 
               conf.media_meta_length, conf.media_meta_length, i->album);
}

void client_init(struct client *__restrict client, pid_t pid, int unix_fd)
{
    client->pid     = pid;
    client->io      = g_io_channel_unix_new(unix_fd);
    client->out_fd  = -1;
    client->err_fd  = -1;
}

void client_destroy(struct client *__restrict client)
{
    g_io_channel_unref(client->io);
    
    if(client->out_fd >= 0)
        close(client->out_fd);
    
    if(client->err_fd >= 0)
        close(client->err_fd);
}

int client_unix_fd(const struct client *__restrict client)
{
    return g_io_channel_unix_get_fd(client->io);
}

void client_set_out_fd(struct client *__restrict client, int fd)
{
    client->out_fd = fd;
}

void client_set_err_fd(struct client *__restrict client, int fd)
{
    client->err_fd = fd;
}

void client_out(const struct client *__restrict client, const char *format, ...)
{
    va_list args;
    
    if(client->out_fd < 0)
        return;
    
    va_start(args, format);
    
    vdprintf(client->out_fd, format, args);
    
    va_end(args);
}

void client_err(const struct client *__restrict client, const char *format, ...)
{
    va_list args;
    
    if(client->err_fd < 0)
        return;
    
    va_start(args, format);
    
    vdprintf(client->err_fd, format, args);
    
    va_end(args);
}

void client_print_volume(struct client *__restrict client, unsigned int vol)
{
    client_out(client, "\tVolume: %u\n", vol);
}

void client_print_current_media(const struct client *__restrict client,
                                const struct media_player *mp)
{
    const struct playlist *pl;
    const struct link *link;
    const struct media *m;
    int i;
    
    pl = media_player_playlist(mp);
    i = 0;
    
    playlist_for_each(pl, link) {
        i += 1;
        
        m = container_of(link, const struct media, link);
        
        if(m == media_player_current_media(mp)) {
            media_player_print_media(client, m, i, conf.current_media_color);
            break;
        }
    }
}

void client_print_media_player_playlist(const struct client *__restrict client, 
                                        const struct media_player *mp)
{
    const struct playlist *pl;
    const struct link *link;
    const struct media *m;
    int i;
    
    pl = media_player_playlist(mp);
    i = 0;

    playlist_for_each(pl, link) {
        i += 1;
        
        m = container_of(link, const struct media, link);
        
        if(m == media_player_current_media(mp))
            media_player_print_media(client, m, i, conf.current_media_color);
        else
            media_player_print_media(client, m, i, conf.default_media_color);
    }
}