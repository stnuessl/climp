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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>

#include <libvci/config.h>

#include "config.h"
#include "color.h"

#define CLIMPD_CONFIG_PATH "~/.climp/climpd.conf"

#define DEFAULT_MEDIA_META_LENGTH    "24"
#define DEFAULT_MEDIA_ACTIVE_COLOR   "green"
#define DEFAULT_MEDIA_PASSIVE_COLOR  "default"
#define DEFAULT_VOLUME               "100"
#define DEFAULT_REPEAT               "false"
#define DEFAULT_SHUFFLE              "false"


struct climpd_config conf;

static const char default_config_text[] = {
    "MediaMetaLength   = 24             \n"
    "MediaActiveColor  = green          \n"
    "MediaPassiveColor = default        \n"
    "Volume            = 100            \n"
    "Repeat            = false          \n"
    "Shuffle           = false          \n"
};

static struct config *config;


static const char *string_to_color(const char *__restrict color)
{
    if(strcasecmp(color, "green") == 0)
        return COLOR_GREEN;
    else if(strcasecmp(color, "red") == 0)
        return COLOR_RED;
    else if(strcasecmp(color, "yellow") == 0)
        return COLOR_YELLOW;
    else if(strcasecmp(color, "blue") == 0)
        return COLOR_BLUE;
    else if(strcasecmp(color, "magenta") == 0)
        return COLOR_MAGENTA;
    else if(strcasecmp(color, "cyan") == 0)
        return COLOR_CYAN;
    else if(strcasecmp(color, "white") == 0)
        return COLOR_WHITE;
    else
        return COLOR_DEFAULT;
}

static bool string_to_bool(const char *__restrict s)
{
    if(strcasecmp(s, "true") == 0)
        return true;
    else if(strcasecmp(s, "yes") == 0)
        return true;
    else if(strcasecmp(s, "on") == 0)
        return true;
    else if(strcasecmp(s, "y") == 0)
        return true;
    else
        return false;
}

int climp_config_init(void)
{
    struct config *config;
    int fd, err;
    
    fd = open(CLIMPD_CONFIG_PATH, O_CREAT | O_EXCL);
    if(fd < 0) {
        if(errno != EEXIST)
            return -errno;
        
    } else {
        
        err = write(fd, default_config_text, sizeof(default_config_text));
        
        close(fd);
        
        if(err < 0)
            return -errno;
    }
    
    config = config_new(CLIMPD_CONFIG_PATH);
    if(!config)
        return -errno;
    

    err = climp_config_reload();
    if(err < 0) {
        config_delete(config);
        return err;
    }
    
    return 0;
}

int climp_config_reload(void)
{
    const char *val;
    int err;
    
    err = config_parse(config);
    if(err < 0)
        return err;
    
    val = config_value(config, "MediaMetaLength");
    if(!val)
        val = DEFAULT_MEDIA_META_LENGTH;
    
    errno = 0;
    conf.media_meta_length = (unsigned int) strtol(val, NULL, 10);
    
    if(errno != 0)
        conf.media_meta_length = 24;
    
    val = config_value(config, "MediaActiveColor");
    if(!val)
        val = DEFAULT_MEDIA_ACTIVE_COLOR;
    
    conf.media_active_color = string_to_color(val);
    
    val = config_value(config, "MediaPassiveColor");
    if(!val)
        val = DEFAULT_MEDIA_PASSIVE_COLOR;
    
    conf.media_passive_color = string_to_color(val);
    
    val = config_value(config, "Volume");
    if(!val)
        val = DEFAULT_VOLUME;
    
    errno = 0;
    conf.volume = (unsigned int) strtol(val, NULL, 10);
    
    if(errno != 0)
        conf.volume = 100;
    
    val = config_value(config, "Repeat");
    if(!val)
        val = DEFAULT_REPEAT;
    
    conf.repeat = string_to_bool(val);
    
    val = config_value(config, "Shuffle");
    if(!val)
        val = DEFAULT_SHUFFLE;
    
    conf.shuffle = string_to_bool(val);
    
    return 0;
}

void climp_config_destroy(void)
{
    config_delete(config);
}
