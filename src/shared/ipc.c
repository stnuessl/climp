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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/sockios.h>

#include <libvci/macro.h>

#include "ipc.h"

const char *ipc_message_id_string(enum message_id id)
{
    static const char *message_names[] = {
        [IPC_MESSAGE_HELLO]    = "IPC_MESSAGE_HELLO",
        [IPC_MESSAGE_GOODBYE]  = "IPC_MESSAGE_GOODBYE",
        [IPC_MESSAGE_OK]       = "IPC_MESSAGE_OK",
        [IPC_MESSAGE_NO]       = "IPC_MESSAGE_NO",
        [IPC_MESSAGE_PLAY]     = "IPC_MESSAGE_PLAY",
        [IPC_MESSAGE_STOP]     = "IPC_ESSAGE_STOP",
        [IPC_MESSAGE_NEXT]     = "IPC_MESSAGE_NEXT",
        [IPC_MESSAGE_PREVIOUS] = "IPC_MESSAGE_PREVIOUS",
        [IPC_MESSAGE_VOLUME]   = "IPC_MESSAGE_VOLUME",
        [IPC_MESSAGE_MUTE]     = "IPC_MESSAGE_MUTE"
    };
    
    return message_names[id];
}

struct message *ipc_message_new(void)
{
    struct message *msg;
    
    msg = malloc(sizeof(*msg));
    if(!msg)
        return NULL;
    
    ipc_message_init(msg);
    
    return msg;
}

void ipc_message_delete(struct message *__restrict msg)
{
    ipc_message_destroy(msg);
    free(msg);
}

void ipc_message_clear(struct message *__restrict msg)
{
    struct cmsghdr *cmsg;
    
    msg->msghdr.msg_control    = msg->fd_buf;
    msg->msghdr.msg_controllen = sizeof(msg->fd_buf);
    
    cmsg = CMSG_FIRSTHDR(&msg->msghdr); 
    
    cmsg->cmsg_level = 0;
    cmsg->cmsg_type  = 0;
    
    ((int *)CMSG_DATA(cmsg))[IPC_MESSAGE_FD_0] = -1;
    ((int *)CMSG_DATA(cmsg))[IPC_MESSAGE_FD_1] = -1;
}

void ipc_message_init(struct message *__restrict msg)
{
    struct cmsghdr *cmsg;
    
    msg->iovec[0].iov_base = &msg->id;
    msg->iovec[0].iov_len  = sizeof(msg->id);
    
    msg->iovec[1].iov_base = msg->arg;
    msg->iovec[1].iov_len  = sizeof(msg->arg);
    
    msg->msghdr.msg_control    = msg->fd_buf;
    msg->msghdr.msg_controllen = sizeof(msg->fd_buf);
    msg->msghdr.msg_iov        = msg->iovec;
    msg->msghdr.msg_iovlen     = ARRAY_SIZE(msg->iovec);
    msg->msghdr.msg_name       = NULL;
    msg->msghdr.msg_namelen    = 0;
    
    cmsg = CMSG_FIRSTHDR(&msg->msghdr); 

    cmsg->cmsg_len   = CMSG_LEN(2 * sizeof(int));
    cmsg->cmsg_level = 0;
    cmsg->cmsg_type  = 0;
    
    ((int *)CMSG_DATA(cmsg))[IPC_MESSAGE_FD_0] = -1;
    ((int *)CMSG_DATA(cmsg))[IPC_MESSAGE_FD_1] = -1;
}

void ipc_message_destroy(struct message *__restrict msg)
{
    memset(msg, 0, sizeof(*msg));
}

void ipc_message_set_id(struct message *__restrict msg, enum message_id id)
{
    msg->id = id;
}

enum message_id ipc_message_id(const struct message *__restrict msg)
{
    return msg->id;
}

int ipc_message_set_arg(struct message *__restrict msg, const char *arg)
{
    size_t size;
    
    if(!arg)
        arg = IPC_MESSAGE_EMPTY_ARG;
    
    size = strlen(arg) + 1;
    
    if(size >= sizeof(msg->arg))
        return -EMSGSIZE;
    
    memcpy(msg->arg, arg, size);
    
    return 0;
}

const char *ipc_message_arg(const struct message *__restrict msg)
{
    return msg->arg;
}

void ipc_message_set_fds(struct message *__restrict msg, int fd0, int fd1)
{
    struct cmsghdr *cmsg;
    
    cmsg = CMSG_FIRSTHDR(&msg->msghdr);
    
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    
    ((int *)CMSG_DATA(cmsg))[IPC_MESSAGE_FD_0] = fd0;
    ((int *)CMSG_DATA(cmsg))[IPC_MESSAGE_FD_1] = fd1;
}

int ipc_message_fd(const struct message *__restrict msg, int i)
{
    struct cmsghdr *cmsg;
    
    cmsg = CMSG_FIRSTHDR(&msg->msghdr);
    
    return ((int *)CMSG_DATA(cmsg))[i];
}

int ipc_send_message(int fd, struct message *__restrict msg)
{
    int err;
    
again:
    err = sendmsg(fd, &msg->msghdr, MSG_NOSIGNAL);
    if(err < 0) {
        if(errno == EINTR)
            goto again;
        
        return -errno;
    }
    
    return 0;
}

int ipc_recv_message(int fd, struct message *msg)
{
    ssize_t err;
    
again:
    err = recvmsg(fd, &msg->msghdr, MSG_NOSIGNAL);
    if(err < 0) {
        if(errno == EINTR)
            goto again;
        
        return -errno;
    }
    
    if(err == 0)
        return -EIO;
    
    if(err != IPC_MESSAGE_SIZE)
        return -EINVAL;
    
    return 0;
}