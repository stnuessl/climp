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

## Bugs and bug reports

You can leave a bug report on the [github project page](https://github.com/stnuessl/climp/issues), 
however don't expect to get them fixed soon. Make sure to leave a note on how to reproduce the bug and 
the output of the application (including the log file if applicable).