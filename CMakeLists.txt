cmake_minimum_required(VERSION 3.14)
project(hand-serial-test VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add submodules as subdirectories
add_subdirectory(external/dartt-protocol)
add_subdirectory(external/serial-cross-platform)
add_subdirectory(external/byte-stuffing)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
	src/abh_communication.c
	src/AbilityHand.cpp
)

# add windows socket libraries if on windows only for tinycsocket
if(WIN32)
    # tinycsocket socket libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE wsock32 ws2_32 iphlpapi)
endif()


# Link serial-cross-platform library
target_link_libraries(${PROJECT_NAME} 
	PRIVATE
	dartt_protocol
    dartt_checksum
	serial
	PPP
	cobs
)

# Include directories if needed
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
	${serial_SOURC_DIR}
)
