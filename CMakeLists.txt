cmake_minimum_required(VERSION 3.14)
project(ability_hand VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(SKBUILD OR BUILD_PYTHON_BINDINGS)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(NOT TARGET serial)
    add_subdirectory(external/serial-cross-platform)
endif()
if(NOT TARGET PPP)
    add_subdirectory(external/byte-stuffing)
endif()
if(NOT TARGET pybind11)
    add_subdirectory(external/pybind11)
endif()

# include(FetchContent)	#uncomment to regenerate tinycsocket files
# FetchContent_Declare(
#     tinycsocket
#     GIT_REPOSITORY https://gitlab.com/dosshell/tinycsocket.git
#     GIT_TAG v0.3  # Use the latest version tag, or master if you want the break your build system in the future
# )
# FetchContent_MakeAvailable(tinycsocket)

if(WIN32)
    set(TINYCSOCKET_PLATFORM_SRC src/tinycsocket_win32.c)
else()
    set(TINYCSOCKET_PLATFORM_SRC src/tinycsocket_posix.c)
endif()

add_library(ability_hand STATIC
    src/AbilityHand.cpp
    src/abh_communication.c
    src/udp.cpp
    src/tinycsocket_common.c
	src/abh_crc.c
    ${TINYCSOCKET_PLATFORM_SRC}
)

target_include_directories(ability_hand PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

if(WIN32)
    target_compile_definitions(ability_hand PRIVATE TINYCSOCKET_USE_WIN32_IMPL)
    target_link_libraries(ability_hand PUBLIC wsock32 ws2_32 iphlpapi)
else()
    # target_compile_definitions(ability_hand PRIVATE TINYCSOCKET_USE_POSIX_IMPL)
endif()

target_link_libraries(ability_hand PUBLIC
    serial
    PPP
    cobs
	bytestream
	# tinycsocket	#uncomment to regenerate tinycsocket files
)

option(BUILD_EXAMPLES "Build example applications" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
if(BUILD_PYTHON_BINDINGS OR SKBUILD)
    add_subdirectory(python)
endif()
