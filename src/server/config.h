#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>

struct climpd_config {
    /* output options */
    unsigned int media_meta_length;
    char *current_media_color;
    char *default_media_color;
    
    /* media player options */
    unsigned int volume;
    bool repeat;
    bool shuffle;
};

int config_init(void);

void config_destroy(void);

#endif /* _CONFIG_H_ */