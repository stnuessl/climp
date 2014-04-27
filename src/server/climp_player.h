#ifndef _CLIMP_PLAYER_H_
#define _CLIMP_PLAYER_H_

#include <libvci/random.h>

#include "media_player/media_player.h"
#include "media_player/playlist.h"
#include "media_player/media.h"

struct climp_player {
    struct media_player mp;
    struct playlist *pl;
    struct media *next;
    
    struct random rand;
    
    bool repeat;
    bool shuffle;
};

struct climp_player *climp_player_new(void);

void climp_player_delete(struct climp_player *__restrict cp);

int climp_player_init(struct climp_player *__restrict cp);

void climp_player_destroy(struct climp_player *__restrict cp);

int climp_player_play_file(struct climp_player *__restrict cp, 
                           const char *__restrict path);

int climp_player_play_media(struct climp_player *__restrict cp, 
                            struct media *m);

int climp_player_play(struct climp_player *__restrict cp);

void climp_player_pause(struct climp_player *__restrict cp);

void climp_player_stop(struct climp_player *__restrict cp);

int climp_player_next(struct climp_player *__restrict cp);

int climp_player_previous(struct climp_player *__restrict cp);

int climp_player_add_file(struct climp_player *__restrict cp, const char *path);

int climp_player_add_media(struct climp_player *__restrict cp, struct media *m);

void climp_player_delete_file(struct climp_player *__restrict cp,
                              const char *path);

void climp_player_delete_media(struct climp_player *__restrict cp,
                               struct media *m);

void climp_player_remove_media(struct climp_player *__restrict cp,
                               const struct media *m);

void climp_player_set_volume(struct climp_player *__restrict cp, 
                             unsigned int volume);

unsigned int climp_player_volume(const struct climp_player *__restrict cp);

void climp_player_set_muted(struct climp_player *__restrict cp, bool muted);

bool climp_player_muted(const struct climp_player *__restrict cp);

void climp_player_set_repeat(struct climp_player *__restrict cp, bool repeat);

bool climp_player_repeat(const struct climp_player *__restrict cp);

void climp_player_set_shuffle(struct climp_player *__restrict cp, bool shuffle);

bool climp_player_shuffle(const struct climp_player *__restrict cp);

#endif /* _CLIMP_PLAYER_H_ */