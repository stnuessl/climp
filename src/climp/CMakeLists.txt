
link_directories(${CMAKE_LIBRARY_PATH})

add_executable(climp 
    main.c
    ../shared//ipc.c
)

target_link_libraries(climp vci)

install(PROGRAMS ${CMAKE_BINARY_DIR}/src/climp/climp
        DESTINATION ${TARGET_INSTALL_DIR})