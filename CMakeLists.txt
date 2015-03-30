cmake_minimum_required(VERSION 2.6)

# project name
project(climp C)

add_definitions(-Wall -Wextra -Wpedantic)
add_definitions(-D_GNU_SOURCE)
#add_definitions(-DNDEBUG)
#add_definitions(-O2) 
add_definitions(-g3)

#set(CMAKE_BUILD_TYPE "release")
#set(CMAKE_BUILD_TYPE "debug")

set(CMAKE_C_FLAGS "-std=c11")
set(CMAKE_INSTALL_PREFIX /usr/local)
set(TARGET_INSTALL_DIR bin)
set(CMAKE_LIBRARY_PATH /usr/local/lib)

set(SOURCE_CLIMP
    src/client/main.c
    src/client/climpd.c
    src/shared/ipc.c
    )
    
set(SOURCE_CLIMPD
    src/server/main.c
    src/server/core/bool-map.c
    src/server/core/climpd-log.c
    src/server/core/climpd-player.c
    src/server/core/climpd-config.c
    src/server/core/media-discoverer.c
    src/server/core/climpd-paths.c
    src/server/core/media-loader.c
    src/server/core/terminal-color-map.c
    src/server/core/socket.c
    src/server/core/util.c
    src/server/core/mainloop.c
    src/server/obj/playlist.c
    src/server/obj/media-list.c
    src/server/obj/media.c
    src/server/obj/kfy.c
    src/server/obj/uri.c
    src/shared/ipc.c
    src/shared/util.c
    )

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB glib-2.0)
pkg_check_modules(GOBJECT gobject-2.0)
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_PBUTILS gstreamer-pbutils-1.0)

# <XPREFIX>_FOUND          ... set to 1 if module(s) exist
# <XPREFIX>_LIBRARIES      ... only the libraries (w/o the '-l')
# <XPREFIX>_LIBRARY_DIRS   ... the paths of the libraries (w/o the '-L')
# <XPREFIX>_LDFLAGS        ... all required linker flags
# <XPREFIX>_LDFLAGS_OTHER  ... all other linker flags
# <XPREFIX>_INCLUDE_DIRS   ... the '-I' preprocessor flags (w/o the '-I')
# <XPREFIX>_CFLAGS         ... all required cflags
# <XPREFIX>_CFLAGS_OTHER   ... the other compiler flags
                    
include_directories(src/server/)
include_directories(src/shared/)
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMER_PBUTILS_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})

link_directories(${CMAKE_LIBRARY_PATH})
link_directories(${GLIB_LIBRARY_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})
link_directories(${GSTREAMER_PBUTILS_LIBRARY_DIRS})

add_executable(climp ${SOURCE_CLIMP})
target_link_libraries(climp vci)

add_executable(climpd ${SOURCE_CLIMPD})
target_link_libraries(climpd ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(climpd ${GLIB_LIBRARIES})
target_link_libraries(climpd ${GSTREAMER_LIBRARIES})
target_link_libraries(climpd ${GSTREAMER_PBUTILS_LIBRARIES})
target_link_libraries(climpd vci)
target_link_libraries(climpd m)

install(PROGRAMS ${CMAKE_BINARY_DIR}/climp ${CMAKE_BINARY_DIR}/climpd 
        DESTINATION ${TARGET_INSTALL_DIR}
        )