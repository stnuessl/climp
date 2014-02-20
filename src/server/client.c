#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "client.h"

void client_init(struct client *__restrict client, pid_t pid, int unix_fd)
{
    client->pid     = pid;
    client->unix_fd = unix_fd;
    client->out_fd  = -1;
    client->err_fd  = -1;
}

void client_destroy(struct client *__restrict client)
{
    close(client->unix_fd);
    
    if(client->out_fd >= 0)
        close(client->out_fd);
    
    if(client->err_fd >= 0)
        close(client->err_fd);
}

int client_unix_fd(const struct client *__restrict client)
{
    return client->unix_fd;
}

void client_set_out_fd(struct client *__restrict client, int fd)
{
    client->out_fd = fd;
}

void client_set_err_fd(struct client *__restrict client, int fd)
{
    client->err_fd = fd;
}

void client_out(struct client *__restrict client, const char *format, ...)
{
    va_list args;
    
    if(client->out_fd < 0)
        return;
    
    va_start(args, format);
    
    vdprintf(client->out_fd, format, args);
    
    va_end(args);
}

void client_err(struct client *__restrict client, const char *format, ...)
{
    va_list args;
    
    if(client->err_fd < 0)
        return;
    
    va_start(args, format);
    
    vdprintf(client->err_fd, format, args);
    
    va_end(args);
}

void client_out_current_track(struct client *__restrict client,
                              struct media_player *__restrict mp)
{
    client_out(client, "Playing\t~~ %s ~~\n", media_player_current_track(mp));
}