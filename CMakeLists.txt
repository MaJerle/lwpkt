cmake_minimum_required(VERSION 3.22)

# Setup project
project(LwLibPROJECT)

if(NOT PROJECT_IS_TOP_LEVEL)
    add_subdirectory(lwpkt)
else()
    # Set as executable
    add_executable(${PROJECT_NAME})

    # Add key executable block
    target_sources(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/dev/main.c
        ${CMAKE_CURRENT_LIST_DIR}/libs/lwrb/src/lwrb/lwrb.c
        ${CMAKE_CURRENT_LIST_DIR}/examples/example_lwpkt.c
        ${CMAKE_CURRENT_LIST_DIR}/examples/example_lwpkt_evt.c
        ${CMAKE_CURRENT_LIST_DIR}/test/lwpkt_test.c
    )

    # Add key include paths
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/dev
        ${CMAKE_CURRENT_LIST_DIR}/libs/lwrb/src/include
    )

    # Compilation definition information
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        WIN32
        _DEBUG
        CONSOLE
        LWPKT_DEV
    )

    # Compiler options
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    # Add subdir with lwpkt and link to project
    set(LWPKT_OPTS_DIR ${CMAKE_CURRENT_LIST_DIR}/dev)
    add_subdirectory(lwpkt)
    target_link_libraries(${PROJECT_NAME} lwpkt)
endif()