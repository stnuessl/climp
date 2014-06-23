# climp - Command Line Interface Music Player

This project is incomplete. You should not use it.

## Overview

## Dependencies

You need to install one of my own created libraries: [libvci](https://www.github.com/stnuessl/climp)
Refer to the documentation there.
Also the needed packages may differ between distributions.
Up to this date (June 2014) I got climp to run under
1. Arch Linux
2. Debian - Testing

with the packages mentioned in the following section.

### Arch Linux

In order to compile climp under Arch Linux you need to install to following
package:
    [gstreamer0.10](https://www.archlinux.org/packages/extra/x86_64/gstreamer0.10/)
    
    # pacman -Syu gstreamer0.10

### Debian

To compile climp under Debian you will need the following packages:
    [libgstreamer-plugins-base0.10-dev](https://packages.debian.org/de/sid/libgstreamer-plugins-base0.10-dev)
    [libgstreamer0.10-dev](https://packages.debian.org/de/jessie/libgstreamer0.10-dev)
    gstreamer0.10
    
    # apt-get install gstreamer0.10 libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev

## Installation

First, make sure you got the following tools installed:
1. gcc
2. make
3. cmake

Second, make sure you installed the above mentioned dependencies...

I never tried to compile climp with a different compiler (than gcc) and I do not know
if this is possible.

```bash
   $ git clone https://github.com/stnuessl/climp
   $ mkdir climp/build
   $ cd climp/build
   $ cmake ../
   $ make
   # make install
```

Notice that '# make install' requires root privileges.

## Usage

Ok so you got climp installed, but how do you use it? 
Here is a short guide providing the most important commands
which may help you get started.

### Setting up and running a playlist

Consider having a some songs in <directory>.
Run the following commands to get a playlist up and running:

    $ climp discover <directory> > my_playlist.txt
    $ climp set-playlist my_playlist.txt
    $ climp play

Ok, so what does that do? First the, __discover__ command prints playable (audio) files
to your standard output. We pipe this into a file to create a playlist for climp.
Second, we load and set the playlist active with __set-playlist__. Finally we start the
climp music player with __play__.
Notice, that you can also __pause__ and __stop__ the player.

### Setting player options

This is a short one. Some time during replay you propably want to change
the volume. Or you want to hear the songs in a random order. Or you want to 
automatically repeat the playlist.
Checkout the following commands:

    $ climp set-shuffle { true, false, 1, 0 }
    $ climp set-repeat { true, false, 1, 0 }
    $ climp set-volume [0 - 100]

### Playing a particular songs

To play a particular song there are two possible ways to achieve this.
For the first one you need to now the file. Run:
    
    $ climp play-file <file>

For the second way you run:

    $ climp get-titles

And pick the number of the song you want to hear. If the output is too long for your
terminal window, notice that you can use grep on the __get-titles__ command:

    $ climp get-titles | grep <artist, album, title>
    
Finally run __play-track__ with the corresponding track number given by __get-titles__,
e.g.:

    $ climp play-track 42

### More to come!