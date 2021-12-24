cmake_minimum_required(VERSION 3.13)

# Debug message
message("LWPKT: Entering lib source CMakeLists.txt")

# Set library name
set(LWPKT_LIB_NAME "lwpkt")

# Register library to the system
add_library(${LWPKT_LIB_NAME} INTERFACE)

# Setup generic source files
target_sources(${LWPKT_LIB_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/lwpkt/lwpkt.c
    )

# Setup include directories
target_include_directories(${LWPKT_LIB_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/include
    )
