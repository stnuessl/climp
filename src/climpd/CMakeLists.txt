find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB glib-2.0)
pkg_check_modules(GOBJECT gobject-2.0)
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_PBUTILS gstreamer-pbutils-1.0)

include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMER_PBUTILS_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})

link_directories(${CMAKE_LIBRARY_PATH})
link_directories(${GLIB_LIBRARY_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})
link_directories(${GSTREAMER_PBUTILS_LIBRARY_DIRS})

include_directories(./)
include_directories(../shared/)

add_executable(climpd 
    main.c
    core/audio-player/audio-player.c
    core/audio-player/gst-engine.c
    core/playlist/kfy.c
    core/playlist/playlist.c
    core/playlist/tag-reader.c
    core/argument-parser.c
    core/climpd-config.c
    core/climpd-log.c
    core/daemonize.c
    core/media-loader.c
    ipc/socket-server.c
    media/media.c
    media/uri.c
    util/bool.c
    util/strconvert.c
    ../shared/ipc.c
)
    
target_link_libraries(climpd 
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_PBUTILS_LIBRARIES}
    vci
    m
)

install(PROGRAMS ${CMAKE_BINARY_DIR}/src/climpd/climpd
        DESTINATION ${TARGET_INSTALL_DIR})