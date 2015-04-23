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



# pkg_check_modules(GLIB glib-2.0)
# pkg_check_modules(GOBJECT gobject-2.0)
# pkg_check_modules(GSTREAMER gstreamer-1.0)
# pkg_check_modules(GSTREAMER_PBUTILS gstreamer-pbutils-1.0)

# <XPREFIX>_FOUND          ... set to 1 if module(s) exist
# <XPREFIX>_LIBRARIES      ... only the libraries (w/o the '-l')
# <XPREFIX>_LIBRARY_DIRS   ... the paths of the libraries (w/o the '-L')
# <XPREFIX>_LDFLAGS        ... all required linker flags
# <XPREFIX>_LDFLAGS_OTHER  ... all other linker flags
# <XPREFIX>_INCLUDE_DIRS   ... the '-I' preprocessor flags (w/o the '-I')
# <XPREFIX>_CFLAGS         ... all required cflags
# <XPREFIX>_CFLAGS_OTHER   ... the other compiler flags
                    

# include_directories(${GSTREAMER_INCLUDE_DIRS})
# include_directories(${GSTREAMER_PBUTILS_INCLUDE_DIRS})
# include_directories(${GLIB_INCLUDE_DIRS})
# 
# link_directories(${CMAKE_LIBRARY_PATH})
# link_directories(${GLIB_LIBRARY_DIRS})
# link_directories(${GSTREAMER_LIBRARY_DIRS})
# link_directories(${GSTREAMER_PBUTILS_LIBRARY_DIRS})

add_subdirectory(${PROJECT_SOURCE_DIR}/src/climpd)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/climp)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/climp-discoverer)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/climp-log)
