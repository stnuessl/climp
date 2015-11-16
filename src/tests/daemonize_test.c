/*
 * Copyright (C) 2015  Steffen Nüssle
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

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <libvci/macro.h>

#include "../climpd/core/climpd-log.h"
#include "../climpd/core/daemonize.h"

bool stop = false;

void on_sighub(int signo, siginfo_t *info, void *context)
{
    (void) signo;
    (void) info;
    (void) context;
}

void on_sigterm(int signo, siginfo_t *info, void *context)
{
    (void) signo;
    (void) info;
    (void) context;
    
    stop = !stop;
}

struct signal_handle handles[] = {
    { SIGQUIT,          SIG_IGN     },
    { SIGINT,           SIG_IGN     },
    { SIGTSTP,          SIG_IGN     },
    { SIGTTIN,          SIG_IGN,    },
    { SIGTTOU,          SIG_IGN,    },
    { SIGPIPE,          SIG_IGN,    },
    { SIGHUP,           &on_sighub  },
    { SIGTERM,          &on_sigterm },
    
};

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    
    printf("Run '$ kill -s SIGERM <pid>' to terminate the process\n");

    assert(climpd_log_init("/tmp/daemonize.log") == 0 && "climpd_log_init");
    
    assert(daemonize(handles, ARRAY_SIZE(handles)) == 0 && "daemonize");
    
    while (!stop)
        sleep(1);
    
    climpd_log_destroy();
    
    return 0;
}