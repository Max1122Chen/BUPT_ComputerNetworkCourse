cmake_minimum_required(VERSION 3.16)

project(datalink C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(DATAlink_SOURCES
    crc32.c
    datalink.c
    getopt.c
    lprintf.c
    protocol.c
    protocols/go_back_n.c
    protocols/go_back_n_piggypacking.c
    protocols/selective_repeat.c
    protocols/stop_and_wait.c
)

add_executable(datalink
    ${DATAlink_SOURCES}
)

target_include_directories(datalink PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(datalink PRIVATE /W3)
endif()

if(WIN32)
    target_link_libraries(datalink PRIVATE wsock32 ws2_32)
endif()
