/* 
 * climp - Command Line Interface Music Player
 * Copyright (C) 2014  Steffen Nüssle
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

#ifndef _CLIMP_PLAYER_H_
#define _CLIMP_PLAYER_H_

int climp_player_init(void);

void climp_player_destroy(void);

int climp_player_play_title(const char *title);

int climp_player_handle_events(void);

const char *climp_error_message(void);

int climp_player_send_message(const char *msg);

#endif /* _CLIMP_PLAYER_H_ */