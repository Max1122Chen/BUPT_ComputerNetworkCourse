cmake_minimum_required(VERSION 3.16)

project(datalink C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB_RECURSE DATAlink_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
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
