#ifndef _CLIMP_PLAYER_H_
#define _CLIMP_PLAYER_H_

int climp_player_init(void);

void climp_player_destroy(void);

int climp_player_play_title(const char *title);

int climp_player_handle_events(void);

const char *climp_error_message(void);

int climp_player_send_message(const char *msg);

#endif /* _CLIMP_PLAYER_H_ */