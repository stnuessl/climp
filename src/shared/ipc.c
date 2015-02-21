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
#include <errno.h>
#include <assert.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/sockios.h>

#include <libvci/buffer.h>

#include "ipc.h"

static int ipc_send(int sock, void *__restrict buf, size_t len)
{
    int err;
    
again:
    err = send(sock, buf, len, MSG_NOSIGNAL);
    if (err < 0) {
        if (errno == EINTR)
            goto again;
        
        return -errno;
    }
    
    return 0;
}

static int ipc_recv(int sock, void *__restrict buf, size_t len)
{
    ssize_t err;
    
again:
    err = recv(sock, buf, len, MSG_NOSIGNAL);
    if (err < 0) {
        if (errno == EINTR)
            goto again;
        
        return -errno;
    }
    
    if (err == 0)
        return -EIO;
    
    return 0;
}

int ipc_send_fds(int sock, int fd_out, int fd_err)
{
    struct msghdr msghdr;
    struct cmsghdr *cmsg;
    char data[CMSG_SPACE(sizeof(fd_out) + sizeof(fd_err))];
    int *fds;
    ssize_t err;
    
    msghdr.msg_control    = data;
    msghdr.msg_controllen = sizeof(data);
    msghdr.msg_iov        = NULL;
    msghdr.msg_iovlen     = 0;
    msghdr.msg_name       = NULL;
    msghdr.msg_namelen    = 0;
    
    cmsg = CMSG_FIRSTHDR(&msg->msghdr);
    
    cmsg->cmsg_len   = msghdr.msg_controllen;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    
    fds = (int *) CMSG_DATA(cmsg);
    fds[0] = fd_out;
    fds[1] = fd_err;
    
again:
    err = sendmsg(sock, &msghdr, MSG_NOSIGNAL);
    if (err < 0) {
        if (errno == EINTR)
            goto again;
        
        return -errno;
    }
    
    return 0;
}

int ipc_recv_fds(int sock, int *__restrict fd_out, int *__restrict fd_err)
{
    struct msghdr msghdr;
    struct cmsghdr *cmsg;
    char data[CMSG_SPACE(sizeof(fd_out) + sizeof(fd_err))];
    int *fds;
    ssize_t err;
    
    msghdr.msg_control    = data;
    msghdr.msg_controllen = sizeof(data);
    msghdr.msg_iov        = NULL;
    msghdr.msg_iovlen     = 0;
    msghdr.msg_name       = NULL;
    msghdr.msg_namelen    = 0;
    
    cmsg = CMSG_FIRSTHDR(&msg->msghdr);
    
    cmsg->cmsg_len   = msghdr.msg_controllen;
    cmsg->cmsg_level = 0;
    cmsg->cmsg_type  = 0;
    
again:
    err = recvmsg(sock &msghdr, MSG_NOSIGNAL);
    if (err < 0) {
        if (errno == EINTR)
            goto again;
    }
    
    if (err == 0)
        return -EIO;
    
    fds = (int *) CMSG_DATA(cmsg);
    *fd_out = fds[0];
    *fd_err = fds[1];

    return 0;
}

int ipc_send_argv(int sock, const char **argv, int argc)
{
    struct buffer buf;
    int i, err;
    size_t buf_size, argv_lengths[argc];
    
    buf_size = sizeof(argc) + sizeof(buf_size);
    
    for (i = 0; i < argc; ++i) {
        argv_lengths[i] = strlen(argv[i]) + 1;
        buf_size += argv_lengths[i];
    }
    
    err = buffer_init(&buf, buf_size);
    if (err < 0)
        return err;
    
    buffer_write_int(&buf, argc);
    buffer_write(&buf, &buf_size, sizeof(buf_size));
    
    for (i = 0; i < argc; ++i)
        buffer_write(&buf, argv[i], argv_lengths[i]);
    
    assert(buffer_size(&buf) == buf_len && "Invalid buffer size");
    
    err = ipc_send(sock, buffer_data(&buf), buffer_size(&buf));
    
    buffer_destroy(&buf);
    
    return err;
}

int ipc_recv_argv(int sock, char ***argv, int *__restrict argc)
{
    static const int arg_max = (int) sysconf(_SC_ARG_MAX);
    char *buf;
    size_t buf_len, size;
    int i, err;
    
    err = ipc_recv(sock, argc, sizeof(*argc));
    if (err < 0)
        return err;
    
    err = ipc_recv(sock, &buf_len, sizeof(buf_len));
    if (err < 0)
        return err;
    
    if (*argc > arg_max)
        return -EINVAL;
    
    /* allocate space for 'argv' array plus all transmitted strings */
    size = *argc * sizeof(*argv) + buf_len - sizeof(*argc) - sizeof(buf_len);
    
    buf = malloc(size);
    if (!buf)
        return -errno;
    
    *argv = (char **) buf;
    /* 
     * leave space for the 'argv' array 
     * and jump to the beginning of the string buffer 
     */
    buf += *argc * sizeof(*argv);
    
    /* read in all strings at once */
    err = ipc_recv(sock, buf, buf_len - sizeof(*argc) - sizeof(buf_len));
    if (err < 0) {
        free(*argv);
        return err;
    }
    
    for (i = 0; i < *argc; ++i, buf = strchr(buf, '\0') + 1)
        (*argv)[i] = buf;
    
    return 0;
}

int ipc_send_status(int sock, int status)
{
    return ipc_send(sock, &status, sizeof(status));
}

int ipc_recv_status(int sock, int *__restrict status)
{
    return ipc_recv(sock, status, sizeof(*status));
}
