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
    core/media-player/media-player.c
    core/media-player/media-discoverer.c
    core/climpd-log.c
    core/climpd-player.c
    core/climpd-config.c
    core/climpd-paths.c
    core/media-loader.c
    core/terminal-color-map.c
    core/socket.c
    core/util.c
    core/mainloop.c
    obj/playlist.c
    obj/media-list.c
    obj/media.c
    obj/kfy.c
    obj/uri.c
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