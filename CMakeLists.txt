cmake_minimum_required(VERSION 2.6)

# project name
project(climp C)

add_definitions(-D_GNU_SOURCE)
add_definitions(-Wall)
#add_definitions(-O2)
add_definitions(-g3)

set(LIBS vci vlc)
set(CMAKE_LIBRARY_PATH /usr/local/lib)
set(SOURCE
    src/main.c
    src/climp_player.c)

link_directories(${CMAKE_LIBRARY_PATH})

add_executable(climp ${SOURCE})
target_link_libraries(climp ${LIBS})