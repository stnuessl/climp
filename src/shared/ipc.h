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

#ifndef _IPC_H_
#define _IPC_H_

#define CLIMP_IPC_SOCKET_PATH           "/tmp/.climp.socket"
#define CLIMP_IPC_MAX_MESSAGE_SIZE      256

enum message_id {
    CLIMP_MESSAGE_HELLO,
    CLIMP_MESSAGE_GOODBYE,
    CLIMP_MESSAGE_OK,
    CLIMP_MESSAGE_NO,
    CLIMP_MESSAGE_PLAY,
    CLIMP_MESSAGE_STOP,
    CLIMP_MESSAGE_NEXT,
    CLIMP_MESSAGE_PREVIOUS,
    CLIMP_MESSAGE_MAX_ID
}; 

struct message {
    enum message_id id;
    char arg[CLIMP_IPC_MAX_MESSAGE_SIZE - sizeof(enum message_id)];
};

const char *climp_ipc_message_id_to_string(enum message_id id);

int climp_ipc_init_message(struct message *__restrict msg, 
                           enum message_id id, 
                           const char *s);

int climp_ipc_send_message(int fd, const struct message *__restrict msg);

int climp_ipc_recv_message(int fd, struct message *__restrict msg);

#endif /* _IPC_H_ */