# 
# This file provides set of variables for end user
# and also generates one (or more) libraries, that can be added to the project using target_link_libraries(...)
#
# Before this file is included to the root CMakeLists file (using include() function), user can set some variables:
#
# LWPKT_OPTS_DIR: If defined, it should set the folder path where options file shall be generated.
# LWPKT_COMPILE_OPTIONS: If defined, it provide compiler options for generated library.
# LWPKT_COMPILE_DEFINITIONS: If defined, it provides "-D" definitions to the library build
#

# Library core sources
set(lwpkt_core_SRCS 
    ${CMAKE_CURRENT_LIST_DIR}/src/lwpkt/lwpkt.c
)

# Setup include directories
set(lwpkt_include_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/src/include
)

# Register library to the system
add_library(lwpkt INTERFACE)
target_sources(lwpkt INTERFACE ${lwpkt_core_SRCS})
target_include_directories(lwpkt INTERFACE ${lwpkt_include_DIRS})
target_compile_options(lwpkt PRIVATE ${LWPKT_COMPILE_OPTIONS})
target_compile_definitions(lwpkt PRIVATE ${LWPKT_COMPILE_DEFINITIONS})

# Create config file
if(DEFINED LWPKT_OPTS_DIR AND NOT EXISTS ${LWPKT_OPTS_DIR}/lwpkt_opts.h)
    configure_file(${CMAKE_CURRENT_LIST_DIR}/src/include/lwpkt/lwpkt_opts_template.h ${LWPKT_OPTS_DIR}/lwpkt_opts.h COPYONLY)
endif()