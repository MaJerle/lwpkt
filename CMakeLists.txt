cmake_minimum_required(VERSION 3.0.0)
project(LwLibPROJECT VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(${PROJECT_NAME}
    lwpkt/src/lwpkt/lwpkt.c
    libs/lwrb/src/lwrb/lwrb.c
    dev/VisualStudio/main.c
    examples/example_lwpkt.c
    examples/example_lwpkt_evt.c
    )

target_include_directories(${PROJECT_NAME} PRIVATE
    dev/VisualStudio
    lwpkt/src/include
    libs/lwrb/src/include
    )

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32
    _DEBUG
    CONSOLE
    LWPKT_DEV
    )

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
