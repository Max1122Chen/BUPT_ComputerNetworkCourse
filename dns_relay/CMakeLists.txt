cmake_minimum_required(VERSION 3.16)

project(dnsrelay C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(DNS_RELAY_COMMON_SOURCES
    src/config.c
    src/debug.c
    src/dns_packet.c
    src/id_map.c
    src/net_util.c
    src/platform_win.c
    src/socket_io.c
)

set(DNS_RELAY_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(dnsrelay
    src/main.c
    src/relay.c
    ${DNS_RELAY_COMMON_SOURCES}
)

target_include_directories(dnsrelay PRIVATE ${DNS_RELAY_INCLUDE_DIR})

if(MINGW OR WIN32)
    target_link_libraries(dnsrelay PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(dnsrelay PRIVATE /W4)
else()
    target_compile_options(dnsrelay PRIVATE -Wall -Wextra)
endif()

option(DNS_RELAY_BUILD_TESTS "Build unit tests" ON)

if(DNS_RELAY_BUILD_TESTS)
    add_executable(dnsrelay_test
        tests/test_main.c
        ${DNS_RELAY_COMMON_SOURCES}
    )
    target_include_directories(dnsrelay_test PRIVATE ${DNS_RELAY_INCLUDE_DIR})
    if(MINGW OR WIN32)
        target_link_libraries(dnsrelay_test PRIVATE ws2_32)
    endif()
    if(NOT MSVC)
        target_compile_options(dnsrelay_test PRIVATE -Wall -Wextra)
    endif()
    enable_testing()
    add_test(NAME dnsrelay_unit COMMAND dnsrelay_test)
endif()
