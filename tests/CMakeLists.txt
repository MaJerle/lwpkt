cmake_minimum_required(VERSION 3.22)

# Setup project
project(LwLibPROJECT)

# Set default compile flags for GCC
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(STATUS "GCC detected, adding compile flags")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

enable_testing()
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../libs/lwrb/src/lwrb/lwrb.c
)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../libs/lwrb/src/include
)

# Include file that can add more sources and prepare lib parameters
include(${TEST_CMAKE_FILE_NAME})

# Add subdir with lwpkt and link to project
add_subdirectory("../lwpkt" lwpkt)
target_link_libraries(${PROJECT_NAME} PUBLIC lwpkt)
target_compile_definitions(lwpkt PUBLIC LWPKT_DEV)
target_include_directories(lwpkt PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../libs/lwrb/src/include)

# Add test
add_test(NAME Test COMMAND $<TARGET_FILE:${CMAKE_PROJECT_NAME}>)
