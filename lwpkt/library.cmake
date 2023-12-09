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