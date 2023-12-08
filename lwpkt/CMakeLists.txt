cmake_minimum_required(VERSION 3.22)

# Register library to the system
add_library(lwpkt INTERFACE)
target_sources(lwpkt INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/lwpkt/lwpkt.c)
target_include_directories(lwpkt INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/include)

# Create config file
if(DEFINED LWPKT_OPTS_DIR AND NOT EXISTS ${LWPKT_OPTS_DIR}/lwpkt_opts.h)
    configure_file(${CMAKE_CURRENT_LIST_DIR}/src/include/lwpkt/lwpkt_opts_template.h ${LWPKT_OPTS_DIR}/lwpkt_opts.h COPYONLY)
endif()
