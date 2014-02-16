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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ipc.h"

const char *climp_ipc_message_id_to_string(enum message_id id)
{
    static const char *message_names[] = {
        [CLIMP_MESSAGE_HELLO]    = "CLIMP_MESSAGE_HELLO",
        [CLIMP_MESSAGE_GOODBYE]  = "CLIMP_MESSAGE_GOODBYE",
        [CLIMP_MESSAGE_OK]       = "CLIMP_MESSAGE_OK",
        [CLIMP_MESSAGE_NO]       = "CLIMP_MESSAGE_NO",
        [CLIMP_MESSAGE_PLAY]     = "CLIMP_MESSAGE_PLAY",
        [CLIMP_MESSAGE_STOP]     = "CLIMP_MESSAGE_STOP",
        [CLIMP_MESSAGE_NEXT]     = "CLIMP_MESSAGE_NEXT",
        [CLIMP_MESSAGE_PREVIOUS] = "CLIMP_MESSAGE_PREVIOUS"
    };
    
    return message_names[id];
}

int climp_ipc_init_message(struct message *__restrict msg, 
                           enum message_id id, 
                           const char *s)
{
    size_t len;
    
    msg->id = id;
    
    if(!s)
        return 0;
    
    len = strlen(s) + 1;
    
    if(len >= sizeof(msg->arg))
        return -EINVAL;
    
    memcpy(msg->arg, s, len);
    
    return 0;
}

int climp_ipc_send_message(int fd, const struct message *msg)
{
    struct msghdr msghdr;
    struct iovec iovec;
    ssize_t err;
    
    iovec.iov_base = (void *) msg;
    iovec.iov_len  = sizeof(*msg);
    
    msghdr.msg_iov        = &iovec;
    msghdr.msg_iovlen     = 1;
    msghdr.msg_name       = NULL;
    msghdr.msg_namelen    = 0;
    msghdr.msg_control    = NULL;
    msghdr.msg_controllen = 0;
    
again:
    err = sendmsg(fd, &msghdr, MSG_NOSIGNAL);
    if(err < 0) {
        if(errno == EINTR)
            goto again;
        
        return -errno;
    }
    
    return 0;
}

int climp_ipc_recv_message(int fd, struct message *msg)
{
    struct msghdr msghdr;
    struct iovec iovec;
    ssize_t err;
    
    iovec.iov_base = msg;
    iovec.iov_len  = sizeof(*msg);
    
    msghdr.msg_iov        = &iovec;
    msghdr.msg_iovlen     = 1;
    msghdr.msg_name       = NULL;
    msghdr.msg_namelen    = 0;
    msghdr.msg_control    = NULL;
    msghdr.msg_controllen = 0;
    
again:
    err = recvmsg(fd, &msghdr, MSG_NOSIGNAL);
    if(err < 0) {
        if(errno == EINTR)
            goto again;
        
        return -errno;
    }
    
    if(err == 0)
        return -EIO;
    
    if(err != sizeof(*msg))
        return -EINVAL;
    
    return 0;
}