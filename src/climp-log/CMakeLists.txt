

add_executable(climp-log 
    main.c
)

install(PROGRAMS ${CMAKE_BINARY_DIR}/src/climp-log/climp-log
        DESTINATION ${TARGET_INSTALL_DIR})