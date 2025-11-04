cmake_minimum_required(VERSION 3.14)
project(hand-serial-test VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch serial-cross-platform repository
FetchContent_Declare(
    serial
    GIT_REPOSITORY https://github.com/ocanath/serial-cross-platform.git
    GIT_TAG master  # You can change this to a specific tag or commit hash
)

FetchContent_MakeAvailable(serial)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Link serial-cross-platform library
target_link_libraries(${PROJECT_NAME} PRIVATE serial)

# Include directories if needed
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
	${serial_SOURC_DIR}
)
