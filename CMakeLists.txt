cmake_minimum_required(VERSION 2.6)

# project name
project(climp C)

add_definitions(-D_GNU_SOURCE)
add_definitions(-Wall)
#add_definitions(-O2)
add_definitions(-g3)

set(CMAKE_INSTALL_PREFIX /usr/local)
set(TARGET_INSTALL_DIR bin)
set(LIBS vci vlc)
set(CMAKE_LIBRARY_PATH /usr/local/lib)
set(SOURCE
    src/client/main.c
    src/client/climpd.c
    src/shared/ipc.c
    )
    
set(SOURCE_SERVER
    src/server/main.c
    src/server/media_player.c
    src/shared/ipc.c
    src/shared/util.c
    )

link_directories(${CMAKE_LIBRARY_PATH})

add_executable(climp ${SOURCE})

add_executable(climpd ${SOURCE_SERVER})
target_link_libraries(climpd ${LIBS})

install(PROGRAMS
        ${CMAKE_BINARY_DIR}/climp
        ${CMAKE_BINARY_DIR}/climpd
        DESTINATION
        ${TARGET_INSTALL_DIR}
        )