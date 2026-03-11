cmake_minimum_required(VERSION 3.14)
project(ability_hand VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET dartt_protocol)
    add_subdirectory(external/dartt-protocol)
endif()
if(NOT TARGET serial)
    add_subdirectory(external/serial-cross-platform)
endif()
if(NOT TARGET PPP)
    add_subdirectory(external/byte-stuffing)
endif()

add_library(ability_hand STATIC
    src/AbilityHand.cpp
    src/abh_communication.c
    src/udp.cpp
)

target_include_directories(ability_hand PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

if(WIN32)
    target_link_libraries(ability_hand PUBLIC wsock32 ws2_32 iphlpapi)
endif()

target_link_libraries(ability_hand PUBLIC
    dartt_protocol
    dartt_checksum
    serial
    PPP
    cobs
)

option(BUILD_EXAMPLES "Build example applications" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
