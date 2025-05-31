# CMake include file

# Add more sources
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../test_main.c
)

# Define test macro
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE "TEST_STATIC_ALL_ENABLED")

# LwPKT options
set(LWPKT_OPTS_FILE ${CMAKE_CURRENT_LIST_DIR}/lwpkt_opts.h)