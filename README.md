# climp - Command Line Interface Music Player

## Overview

* [Dependencies](https://github.com/stnuessl/climp#dependencies)
    * [Arch Linux](https://github.com/stnuessl/climp#arch-linux)
    * [Debian](https://github.com/stnuessl/climp#debian-jessie)
* [Installation](https://github.com/stnuessl/climp#installation)
* [Usage](https://github.com/stnuessl/climp#usage)
    * [Play media file(s)](https://github.com/stnuessl/climp#play-media-file(s))
    * [Play a webradio](https://github.com/stnuessl/climp#play-a-webradio)
    * [Play a specific track](https://github.com/stnuessl/climp#play-a-specific-track)
    * [Compose a playlist](https://github.com/stnuessl/climp#compose-a-playlist)
    * [Adding files to your playlist](https://github.com/stnuessl/climp#adding-files-to-your-playlist)
    * [Print current playlist](https://github.com/stnuessl/climp#print-current-playlist)
    * [Load a playlist and start playback](https://github.com/stnuessl/climp#load-a-playlist-and-start-playback)
    * [Discover media files and use them to create a playlist and play it](https://github.com/stnuessl/climp#discover-media-files-and-use-them-to-create-a-playlist-and-play-it)
    * [Using the playlist directory](https://github.com/stnuessl/climp#using-the-playlist-directory)
    * [Quitting climp](https://github.com/stnuessl/climp#quitting-climp)
    * [Getting help](https://github.com/stnuessl/climp#getting-help)
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

* [gstreamer](https://www.archlinux.org/packages/extra/x86_64/gstreamer/)
* [gst-plugins-base-libs](https://www.archlinux.org/packages/extra/x86_64/gst-plugins-base-libs/)

To be capable of playing files climp needs additional gstreamer plugins,
so it is probably best to install the following packages too:

* [gst-plugins-bad](https://www.archlinux.org/packages/extra/x86_64/gst-plugins-bad/)
* [gst-plugins-base](https://www.archlinux.org/packages/extra/x86_64/gst-plugins-base/)
* [gst-plugins-good](https://www.archlinux.org/packages/extra/x86_64/gst-plugins-good/)
* [gst-plugins-ugly](https://www.archlinux.org/packages/extra/x86_64/gst-plugins-ugly/)

Run the following command to install all the packages.

```
# pacman -Syu gstreamer gst-plugins-bad gst-plugins-base gst-plugins-good gst-plugins-ugly 
```

### Debian (Jessie)

To compile climp under Debian you will need the following packages:

* gstreamer1.0
* libgstreamer1.0-dev
* libgstreamer-plugins-base1.0-dev

```
# apt-get install gstreamer1.0 libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
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

Here a just some short examples on how to use climp. Hence, not all features of
climp will be covered here. 

### Play media file(s)

    climp --play /path/to/some/file ../some/other/file
or using an URI:

    climp --play file:///some/path/to/a/file

### Play a webradio

    climp --play http://what.ever.com/where/ever
    
### Play a specific track in the playlist

    climp --play <position in playlist>
    
This will also work with negativ integers, where -1 specifies the last track in the playlist.
Notice that the first track is at index zero.
    
### Compose a playlist

    climp --playlist <file1> <file2> ... ../my-playlist.m3u /my/other/playlist.txt ...

### Adding files to your playlist

    climp --add <file1> <file2> ... ../my-playlist.m3u /my/other/playlist.txt ...
    
### Print current playlist

    climp --playlist

### Load a playlist and start playback

    climp --playlist my-playlist.m3u --play
    
### Discover media files and use them to create a playlist and play it

    climp-discover /home/user/Music ../somewhere/else | sort -V | climp --stdin --play
This command works recursively for all encountered subdirectories. Hence, this may
take a while.

### Using the playlist directory

climp has a playlist directory under ~/.config/climp/playlists.
If you put a file there you don't have to specifiy the whole path to climp, e.g.
consider there is a file named my-playlist.m3u in this folder and your working directory
is ~/. Then you can just run:

    climp --playlist my-playlist.m3u
or:
    
    climp --play my-playlist.m3u
or:

    climp --add my-playlist.m3u


### Quitting climp

    climp --quit
or:

    killall -s 15 climpd

If for some reasons this does not work, use:

    killall -s 9 climpd

### Getting help

    climp --help

## Bugs and bug reports

You can leave a bug report on the [github project page](https://github.com/stnuessl/climp/issues), 
however don't expect to get them fixed soon. Make sure to leave a note on how to reproduce the bug and 
the output of the application (including the log file if applicable).