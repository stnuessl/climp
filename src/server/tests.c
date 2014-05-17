#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "media-player/media.h"
#include "media-player/playlist.h"
#include "media-player/media-player.h"
#include "climpd-player.h"


void test_playlist(void)
{
    fprintf(stdout, "Testing playlist... ");
    
    fprintf(stdout, "\t\t[ DONE ]\n");
}

void test_media_player(void)
{
    fprintf(stdout, "Testing media player... ");
    
    fprintf(stdout, " \t[ DONE ]\n");
}

void test_climp_player(void)
{
    fprintf(stdout, "Testing climp player... ");
    
    fprintf(stdout, "\t[ DONE ]\n");
}

int main(int argc, char *argv[])
{
    test_playlist();
    test_media_player();
    test_climp_player();
    
    return EXIT_SUCCESS;
}