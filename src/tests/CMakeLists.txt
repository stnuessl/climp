cmake_minimum_required(VERSION 2.6)

# project name
project(climp C)

add_definitions(-Wall -Wextra -Wpedantic)
add_definitions(-D_GNU_SOURCE)
add_definitions(-std=c11)
add_definitions(-g3)
#add_definitions(-DNDEBUG)
#add_definitions(-O2) 

set(CMAKE_LIBRARY_PATH /usr/local/lib)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB glib-2.0)
pkg_check_modules(GOBJECT gobject-2.0)
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_PBUTILS gstreamer-pbutils-1.0)

include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMER_PBUTILS_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(../climpd/)

link_directories(${CMAKE_LIBRARY_PATH})
link_directories(${GLIB_LIBRARY_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})
link_directories(${GSTREAMER_PBUTILS_LIBRARY_DIRS})

#######################################################

add_executable(daemonize_test
    daemonize_test.c
    ../climpd/core/climpd-log.c
    ../climpd/core/daemonize.c
)

target_link_libraries(daemonize_test vci)

#######################################################

add_executable(playlist_test
    playlist_test.c
    ../climpd/core/climpd-log.c
    ../climpd/core/playlist/playlist.c
    ../climpd/core/playlist/kfy.c
    ../climpd/core/playlist/tag-reader.c
    ../climpd/media/media.c
    ../climpd/media/uri.c
)

target_link_libraries(playlist_test
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_PBUTILS_LIBRARIES}
    vci
)

#######################################################

add_executable(audio_player_test
    audio_player_test.c
    ../climpd/core/climpd-log.c
    ../climpd/core/audio-player/audio-player.c
    ../climpd/core/audio-player/gst-engine.c
    ../climpd/core/playlist/playlist.c
    ../climpd/core/playlist/kfy.c
    ../climpd/core/playlist/tag-reader.c
    ../climpd/media/media.c
    ../climpd/media/uri.c
    ../climpd/util/strconvert.c
)

target_link_libraries(audio_player_test
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_PBUTILS_LIBRARIES}
    vci
    m
)