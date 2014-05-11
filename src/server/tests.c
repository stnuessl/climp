#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "media_player/media.h"
#include "media_player/playlist.h"
#include "media_player/media_player.h"
#include "climp_player.h"


void test_playlist(void)
{
    fprintf(stdout, "Testing playlist... ");
    
    fprintf(stdout, "[ DONE ]\n");
}

void test_media_player(void)
{
    fprintf(stdout, "Testing media player... ");
    
    fprintf(stdout, "[ DONE ]\n");
}

void test_climp_player(void)
{
    fprintf(stdout, "Testing climp player... ");
    
    fprintf(stdout, "[ DONE ]\n");
}

int main(int argc, char *argv[])
{
    test_playlist();
    test_media_player();
    test_climp_player();
    
    return EXIT_SUCCESS;
}