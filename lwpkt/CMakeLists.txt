cmake_minimum_required(VERSION 3.22)

# Register library to the system
add_library(lwpkt INTERFACE)
target_sources(lwpkt INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/lwpkt/lwpkt.c)
target_include_directories(lwpkt INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/include)
