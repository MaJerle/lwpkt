cmake_minimum_required(VERSION 3.22)

# Setup project
project(LwLibPROJECT)

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

# Add compile options to the library, which will propagate options to executable through public link
target_compile_options(lwpkt PUBLIC -Wall -Wextra -Wpedantic)
target_include_directories(lwpkt PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../libs/lwrb/src/include)

# Add test
add_test(NAME Test COMMAND $<TARGET_FILE:${CMAKE_PROJECT_NAME}>)
