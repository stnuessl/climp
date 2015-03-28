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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <execinfo.h>

#include <gst/gst.h>
#include <glib-unix.h>

#include <libvci/error.h>
#include <libvci/macro.h>

#include <core/climpd-log.h>
#include <core/util.h>
#include <core/climpd-config.h>

static const char *tag = "mainloop";

static GMainLoop *mloop;

static void handle_error_signal(int sig)
{
#define STACKTRACE_BUFFER_SIZE 32
    static void *buffer[STACKTRACE_BUFFER_SIZE];
    int size;
    
    climpd_log_e(tag, "received signal \"%s\"\nbacktrace:\n", strsignal(sig));
    
    size = backtrace(buffer, STACKTRACE_BUFFER_SIZE);
    if(size > 0)
        backtrace_symbols_fd(buffer, size, climpd_log_fd());
    
    exit(EXIT_FAILURE);
}

static gboolean handle_signal(void *data)
{
    int sig, err;
    const char *str;
    
    sig = (int)(long) data;
    str = strsignal(sig);
    
    switch(sig) {
    case SIGTERM:
        climpd_log_i(tag, "terminating on signal \"%s\"\n", str);
        g_main_loop_quit(mloop);
        break;
    case SIGHUP:
        climpd_log_i(tag, "reloading config on signal \"%s\"\n", str);
        err = climpd_config_reload();
        if (err < 0)
            climpd_log_w(tag, "failed to reload config - %s\n", strerr(-err));
        break;
    default:
        break;
    }
    
    return true;
}

static void close_std_streams(void)
{
    int streams[] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };
    int fd, err;
    
    fd = open("/dev/null", O_RDWR);
    if(fd < 0) {
        climpd_log_w(tag, "failed to open \"/dev/null\" for redirecting - %s - "
                    "falling back to closing standard streams\n", errstr);
        goto backup;
    }
    
    for(unsigned int i = 0; i < ARRAY_SIZE(streams); ++i) {
        err = dup2(fd, streams[i]);
        if(err < 0) {
            climpd_log_w(tag, "failed to redirect standard stream %d - %s - "
                        "falling back to closing standard streams\n", 
            streams[i], errstr);
            goto backup;
        }
    }
    
    close(fd);
    
    return;
    
backup:
    close(fd);
    
    for (unsigned int i = 0; i < ARRAY_SIZE(streams); ++i)
        close(streams[i]);
}

static void init_signal_handlers_glib(void)
{
    struct {
        int id;
        gboolean (*handler)(void *);
    } sig[] = {
        { SIGTERM,      &handle_signal  },
        { SIGHUP,       &handle_signal  },
    };
    
    for (unsigned int i = 0; i < ARRAY_SIZE(sig); ++i) {
        int id = sig[i].id;
        
        guint eid = g_unix_signal_add(id, sig[i].handler, (void *)(long) id);
        if (eid == 0) {
            climpd_log_e(tag, "\"%s\" - failed to setup signal handler\n", 
                         strsignal(sig[i].id));
            goto fail;
        }
    }
    
    return;
    
fail:
    die_failed_init(tag);
}

static void init_signal_handlers_unix(void)
{
    struct sigaction sa;
    const int error_signals[] = { 
        SIGILL, SIGBUS, SIGSEGV, SIGFPE, SIGPIPE, SIGSYS 
    };
    static const int signals[] = { 
        SIGQUIT, SIGINT, SIGTSTP, SIGTTIN, SIGTTOU 
    };
    int err;
    
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = &handle_error_signal;
    sigemptyset(&sa.sa_mask);
    
    for(unsigned int i = 0; i < ARRAY_SIZE(error_signals); ++i) {
        err = sigaction(error_signals[i], &sa, NULL);
        if(err < 0) {
            climpd_log_e(tag, "failed to set up signal handler for \"%s\" - "
                         "%s\n", strsignal(error_signals[i]), errstr);
            goto fail;
        }
    }
    
    for(unsigned int i = 0; i < ARRAY_SIZE(signals); ++i) {
        err = sigignore(signals[i]);
        if(err < 0) {
            climpd_log_e(tag, "failed to set up signal handler for \"%s\" - "
                         "%s\n", strsignal(error_signals[i]), errstr);
            goto fail;
        }
    }
    
    return;
    
fail:
    die_failed_init(tag);
}


static void init_signal_handlers(void)
{
    init_signal_handlers_unix();
    init_signal_handlers_glib();
}

static int daemonize(void)
{
    int err;
    pid_t pid, sid;
    
    pid = fork();
    if(pid < 0)
        return -errno;
    
    if(pid > 0)
        _exit(EXIT_SUCCESS);
    
    /*
     * We don't want to get closed when our tty is closed.
     * Creating our own session will prevent this.
     */
    sid = setsid();
    if(sid < 0)
        return -errno;
    
    /* 
     * Exit session leading process:
     * Only a session leading process is able to acquire
     * a controlling terminal
     */
    pid = fork();
    if(pid < 0)
        return -errno;
    
    if(pid > 0)
        _exit(EXIT_SUCCESS);
    
    umask(0);
    
    err = chdir("/");
    if(err < 0)
        return -errno;
    
    init_signal_handlers();
    close_std_streams();
    
    return 0;
}

void mainloop_init(bool no_daemon)
{
    mloop = g_main_loop_new(NULL, false);
    if (!mloop)
        goto fail;
    
    if (!no_daemon) {
        int err = daemonize();
        if (err < 0) {
            climpd_log_e(tag, "failed to daemonize - %s\n", strerr(-err));
            goto fail;
        }
    }
    
    climpd_log_i(tag, "initialized\n");
    
    return;
    
fail:
    die_failed_init(tag);
}

void mainloop_destroy(void)
{
    g_main_loop_unref(mloop);
    climpd_log_i(tag, "destroyed\n");
}

void mainloop_run(void)
{
    g_main_loop_run(mloop);
}

void mainloop_quit(void)
{
    g_main_loop_quit(mloop);
    climpd_log_i(tag, "quitting mainloop\n");
}