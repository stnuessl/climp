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

#define IPC_SOCKET_PATH "/tmp/.climp-unix"

int ipc_send_setup(int sock, int fd_in, int fd_out, int fd_err,
                   const char *__restrict wd);

int ipc_recv_setup(int sock, int *fd_in, int *fd_out, int *fd_err, char **wd);

int ipc_send_argv(int sock, const char **argv, int argc);

int ipc_recv_argv(int sock, char ***argv, int *argc);

int ipc_send_status(int sock, int status);

int ipc_recv_status(int sock, int *__restrict status);

#endif /* _IPC_H_ */