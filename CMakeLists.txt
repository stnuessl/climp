cmake_minimum_required(VERSION 2.6)

# project name
project(climp C)

add_definitions(`pkg-config --cflags --libs gstreamer-pbutils-0.10 gstreamer-0.10`)
add_definitions(-D_GNU_SOURCE)
#add_definitions(-DNDEBUG)
add_definitions(-Wall)
#add_definitions(-O2)
add_definitions(-g3)


set(CMAKE_INSTALL_PREFIX /usr/local)
set(TARGET_INSTALL_DIR bin)

set(CLIMPD_LIBS 
    vci 
    gstreamer-0.10 
    gstpbutils-0.10 
    gobject-2.0 
    gmodule-2.0 
    gthread-2.0 
    glib-2.0 
    xml2
    )
    
set(CLIMP_LIBS vci)
set(CMAKE_LIBRARY_PATH /usr/local/lib)
set(SOURCE
    src/client/main.c
    src/client/climpd.c
    src/shared/ipc.c
    )
    
set(SOURCE_SERVER
    src/server/main.c
    src/server/util/terminal-color-map.c
    src/server/util/bool-map.c
    src/server/util/climpd-log.c
    src/server/core/playlist-manager.c
    src/server/core/message-handler.c
    src/server/core/climpd-player.c
    src/server/core/climpd-config.c
    src/server/core/media-manager.c
    src/server/core/media-scheduler.c
    src/server/core/playlist.c
    src/server/core/media.c
    src/shared/ipc.c
    src/shared/util.c
    )

find_package(Threads)

include_directories(src/server/)
include_directories(src/shared/)
link_directories(${CMAKE_LIBRARY_PATH})

add_executable(climp ${SOURCE})
target_link_libraries(climp ${CLIMP_LIBS})

add_executable(climpd ${SOURCE_SERVER})
target_link_libraries(climpd ${CLIMPD_LIBS} ${CMAKE_THREAD_LIBS_INIT})

install(PROGRAMS
        ${CMAKE_BINARY_DIR}/climp
        ${CMAKE_BINARY_DIR}/climpd
        DESTINATION
        ${TARGET_INSTALL_DIR}
        )