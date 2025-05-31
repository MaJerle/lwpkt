cmake_minimum_required(VERSION 3.22)

# Setup project
project(LwLibPROJECT)

if(NOT PROJECT_IS_TOP_LEVEL)
    add_subdirectory(lwpkt)
else()
    set(TEST_CMAKE_FILE_NAME ${CMAKE_CURRENT_LIST_DIR}/tests/dynamic/cmake.cmake)
    add_subdirectory(tests)
endif()