# climp - Command Line Interface Music Player

## Overview

* [Dependencies](https://github.com/stnuessl/climp#dependencies)
    * [Arch Linux](https://github.com/stnuessl/climp#arch-linux)
    * [Debian](https://github.com/stnuessl/climp#debian-jessie)
* [Installation](https://github.com/stnuessl/climp#installation)
* [Usage](https://github.com/stnuessl/climp#usage)
    * [Set up and run a playlist](https://github.com/stnuessl/climp#set-up-and-run-a-playlist)
    * [Set player options](https://github.com/stnuessl/climp#set-player-options)
    * [Play a particular song](https://github.com/stnuessl/climp#play-a-particular-song)
    * [Remove a song](https://github.com/stnuessl/climp#remove-a-song)
    * [Configuration](https://github.com/stnuessl/climp#configuration)
* [Bugs and bug reports](https://github.com/stnuessl/climp#bugs-and-bug-reports)

## Dependencies

You need to install one of my own created libraries: [libvci](https://www.github.com/stnuessl/climp)
Refer to the documentation there.
Also the needed packages may differ between distributions.
Up to this date (June 2014) I got climp to run under

1. Arch Linux
2. Debian (Jessie)

with the packages mentioned in the following section.

### Arch Linux

In order to compile climp under Arch Linux you need to install to following
package:

* [gstreamer0.10](https://www.archlinux.org/packages/extra/x86_64/gstreamer0.10/)

To be capable of playing files climp needs additional gstreamer plugins,
so it is probably best to install the following packages too:

* [gstreamer0.10-bad-plugins](https://www.archlinux.org/packages/extra/x86_64/gstreamer0.10-bad-plugins/)
* [gstreamer0.10-base-plugins](https://www.archlinux.org/packages/extra/x86_64/gstreamer0.10-base-plugins/)
* [gstreamer0.10-good-plugins](https://www.archlinux.org/packages/extra/x86_64/gstreamer0.10-good-plugins/)
* [gstreamer0.10-ffmpeg](https://www.archlinux.org/packages/extra/x86_64/gstreamer0.10-ffmpeg/)
* [gstreamer0.10-ugly-plugins](https://www.archlinux.org/packages/extra/x86_64/gstreamer0.10-ugly-plugins/)

Run the following command to install all the packages.

```
# pacman -Syu gstreamer0.10 gstreamer0.10-bad-plugins gstreamer0.10-base-plugins gstreamer0.10-good-plugins gstreamer0.10-ffmpeg gstreamer0.10-ugly-plugins 
```

### Debian (Jessie)

To compile climp under Debian you will need the following packages:

* [libgstreamer-plugins-base0.10-dev](https://packages.debian.org/de/sid/libgstreamer-plugins-base0.10-dev)
* [libgstreamer0.10-dev](https://packages.debian.org/de/jessie/libgstreamer0.10-dev)
* gstreamer0.10

```
# apt-get install gstreamer0.10 libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev
```

## Installation

First, make sure you got the following tools installed:

1. gcc
2. make
3. cmake

Second, make sure you installed the above mentioned dependencies...

I never tried to compile climp with a different compiler (than gcc) and I do not know
if this is possible.

    $ git clone https://github.com/stnuessl/climp
    $ mkdir climp/build
    $ cd climp/build
    $ cmake ../
    $ make
    # make install

Notice that '# make install' requires root privileges.

## Usage

Ok so you got climp installed, but how do you use it? 
Here is a short guide providing the most important commands
which may help you getting started.

### Set up and run a playlist

Consider having a some songs in <directory>.
Run the following commands to get a playlist up and running:

    $ climp discover <directory> > my_playlist.txt
    $ climp set-playlist my_playlist.txt
    $ climp play

Ok, so what does that do? First the, __discover__ command prints playable (audio) files
to your standard output. We pipe this into a file to create a playlist for climp.
Second, we load and set the playlist active with __set-playlist__. Finally, we start the
climp music player with __play__.
Notice, that you can also __pause__ and __stop__ the player.
Alternativley, you don't have to use __discover__ to create a playlist.  
A playlist is just a file where each line points (with an absolute path!) 
to a file on your file system. The following command can also be used to
create a playlist:

    $ find /abs/path/to/music/folder -iname *.mp3 [-o -iname *.file-ending] > my_playlist.txt

This command is also a lot faster than __discover__,
however, there won't be any file checks from the climpd until it actually tries
to play the returned files.
In both cases you can consider piping the output to the __sort__ command to order the files appropriatly.

### Set player options

This is a short one. Some time during replay you propably want to change
the volume. Or you want to hear the songs in a random order. Or you want to 
automatically repeat the playlist.
Checkout the following commands:

    $ climp set-shuffle { true, false, yes, no, on, off, y, n, 1, 0 }
    $ climp set-repeat { true, false, yes, no, on, off, y, n, 1, 0 }
    $ climp set-volume [0 - 100]

### Play a particular songs

To play a particular song there are two possible ways to achieve this.
For the first one you need to now the file. Run:
    
    $ climp play-file <file>

For the second way you run:

    $ climp get-playlist

And pick the number of the song you want to hear. If the output is too long for your
terminal window, notice that you can use _grep_ on the __get-titles__ command:

    $ climp get-playlist | grep { artist, album, title }
    
Finally run __play-track__ with the corresponding track number given by __get-titles__,
e.g.:

    $ climp play-track 42

### Remove a song

The approach for this one is the same as for playing a particular song. Find out the number
of the song in the playlist and run:

    $ climp remove-track <number>
    
### Configuration

The climpd can be controlled through a simple configuration file located under ~/.config/climp/climpd.conf.
The configuration file looks like this:

    DefaultPlaylist = /abs/path/to/playlist
    MediaMetaLength = 32
    MediaActiveColor = green
    MediaPassiveColor = default
    Volume = 60
    Repeat = true
    Shuffle = true

If a default playlist is set a simple __climp play__ command will start the playback.
The __Media*__ parameters control the output of the __get-playlist__ command. __MediaMetaLength__ influences
the title, artist and album column width of the output. __MediaActiveColor__ defines the color in which the currently
running track will be highlighted, whereas __MediaPassiveColor__ defines how the rest of the output shall be
colored. Run __climp get-colors__ to get a list of supported colors.

## Bugs and bug reports

You can leave a bug report on the [github project page](https://github.com/stnuessl/climp/issues), 
however don't expect to get them fixed soon. Make sure to leave a note on how to reproduce the bug and 
the output of the application (including the log file if applicable).