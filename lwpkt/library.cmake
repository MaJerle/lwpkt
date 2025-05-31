# 
# LIB_PREFIX: LWPKT
#
# This file provides set of variables for end user
# and also generates one (or more) libraries, that can be added to the project using target_link_libraries(...)
#
# Before this file is included to the root CMakeLists file (using include() function), user can set some variables:
#
# LWPKT_OPTS_FILE: If defined, it is the path to the user options file. If not defined, one will be generated for you automatically
# LWPKT_COMPILE_OPTIONS: If defined, it provide compiler options for generated library.
# LWPKT_COMPILE_DEFINITIONS: If defined, it provides "-D" definitions to the library build
#

# Custom include directory
set(LWPKT_CUSTOM_INC_DIR ${CMAKE_CURRENT_BINARY_DIR}/lib_inc)

# Library core sources
set(lwpkt_core_SRCS
    ${CMAKE_CURRENT_LIST_DIR}/src/lwpkt/lwpkt.c
)

# Setup include directories
set(lwpkt_include_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/src/include
    ${LWPKT_CUSTOM_INC_DIR}
)

# Register library to the system
add_library(lwpkt)
target_sources(lwpkt PRIVATE ${lwpkt_core_SRCS})
target_include_directories(lwpkt PUBLIC ${lwpkt_include_DIRS})
target_compile_options(lwpkt PRIVATE ${LWPKT_COMPILE_OPTIONS})
target_compile_definitions(lwpkt PRIVATE ${LWPKT_COMPILE_DEFINITIONS})

# Create config file if user didn't provide one info himself
if(NOT LWPKT_OPTS_FILE)
    message(STATUS "Using default lwpkt_opts.h file")
    set(LWPKT_OPTS_FILE ${CMAKE_CURRENT_LIST_DIR}/src/include/lwpkt/lwpkt_opts_template.h)
else()
    message(STATUS "Using custom lwpkt_opts.h file from ${LWPKT_OPTS_FILE}")
endif()
configure_file(${LWPKT_OPTS_FILE} ${LWPKT_CUSTOM_INC_DIR}/lwpkt_opts.h COPYONLY)
