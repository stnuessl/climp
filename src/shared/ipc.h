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

#ifndef _IPC_H_
#define _IPC_H_

#include <sys/socket.h>
#include <sys/un.h>

#define IPC_SOCKET_PATH         "/tmp/.climp-unix"

#define IPC_MESSAGE_SIZE        512
#define IPC_MESSAGE_FD_0        0
#define IPC_MESSAGE_FD_1        1
#define IPC_MESSAGE_EMPTY_ARG   ""

enum message_id {
    IPC_MESSAGE_HELLO,
    IPC_MESSAGE_GOODBYE,
    IPC_MESSAGE_OK,
    IPC_MESSAGE_NO,
    
    IPC_MESSAGE_SHUTDOWN,
    
    IPC_MESSAGE_DISCOVER,
    
    IPC_MESSAGE_MUTE,
    IPC_MESSAGE_PAUSE,
    IPC_MESSAGE_PLAY,
    IPC_MESSAGE_STOP,
    
    IPC_MESSAGE_GET_COLORS,
    IPC_MESSAGE_GET_CONFIG,
    IPC_MESSAGE_GET_FILES,
    IPC_MESSAGE_GET_PLAYLISTS,
    IPC_MESSAGE_GET_STATE,
    IPC_MESSAGE_GET_TITLES,
    IPC_MESSAGE_GET_VOLUME,
    IPC_MESSAGE_GET_LOG,
    
    IPC_MESSAGE_SET_PLAYLIST,
    IPC_MESSAGE_SET_REPEAT,
    IPC_MESSAGE_SET_SHUFFLE,
    IPC_MESSAGE_SET_VOLUME,
    
    IPC_MESSAGE_PLAY_FILE,
    IPC_MESSAGE_PLAY_NEXT,
    IPC_MESSAGE_PLAY_PREVIOUS,
    IPC_MESSAGE_PLAY_TRACK,
    
    IPC_MESSAGE_LOAD_CONFIG,
    IPC_MESSAGE_LOAD_MEDIA,
    IPC_MESSAGE_LOAD_PLAYLIST,
    
    IPC_MESSAGE_REMOVE_MEDIA,
    IPC_MESSAGE_REMOVE_PLAYLIST,
    
    IPC_MESSAGE_MAX_ID
};

struct message {
    struct msghdr msghdr;
    struct iovec iovec[2];
    enum message_id id;
    char arg[IPC_MESSAGE_SIZE - sizeof(enum message_id)];
    char fd_buf[CMSG_SPACE(2 * sizeof(int))];
};

const char *ipc_message_id_string(enum message_id id);

struct message *ipc_message_new(void);

void ipc_message_delete(struct message *__restrict msg);

void ipc_message_init(struct message *__restrict msg);

void ipc_message_destroy(struct message *__restrict msg);

void ipc_message_clear(struct message *__restrict msg);

void ipc_message_set_id(struct message *__restrict msg, enum message_id id);

enum message_id ipc_message_id(const struct message *__restrict msg);

int ipc_message_set_arg(struct message *__restrict msg, const char *arg);

const char *ipc_message_arg(const struct message *__restrict msg);

void ipc_message_set_fds(struct message *__restrict msg, int fd0, int fd1);

int ipc_message_fd(const struct message *__restrict msg, int i);

int ipc_send_message(int fd, struct message *__restrict msg);

int ipc_recv_message(int fd, struct message *__restrict msg);

#endif /* _IPC_H_ */