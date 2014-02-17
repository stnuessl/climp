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
    IPC_MESSAGE_HELLO,
    IPC_MESSAGE_GOODBYE,
    IPC_MESSAGE_OK,
    IPC_MESSAGE_NO,
    IPC_MESSAGE_PLAY,
    IPC_MESSAGE_STOP,
    IPC_MESSAGE_NEXT,
    IPC_MESSAGE_PREVIOUS,
    IPC_MESSAGE_VOLUME,
    IPC_MESSAGE_MUTE,
    IPC_MESSAGE_ADD,
    IPC_MESSAGE_SHUTDOWN,
    IPC_MESSAGE_MAX_ID
}; 

struct message {
    enum message_id id;
    char arg[CLIMP_IPC_MAX_MESSAGE_SIZE - sizeof(enum message_id)];
};

const char *ipc_message_id_string(enum message_id id);

int ipc_send_message(int fd, 
                     struct message *msg, 
                     enum message_id id, 
                     const char *arg);

int ipc_recv_message(int fd, struct message *__restrict msg);

#endif /* _IPC_H_ */