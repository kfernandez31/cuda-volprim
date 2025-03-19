cmake_minimum_required(VERSION 3.16)
project(MainProject LANGUAGES C CXX)

# Set C++ and C standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set compiler paths manually if needed
set(CMAKE_CXX_COMPILER "/opt/homebrew/bin/g++-14")
set(CMAKE_C_COMPILER "/opt/homebrew/bin/gcc-14")

# Set common flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp -iquote include -I/opt/homebrew/include -Wall -Wextra -O2 -DGLM_ENABLE_EXPERIMENTAL -flax-vector-conversions")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Iinclude -Wall -Wextra -O3")

# Directories
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(BUILD_DIR "${CMAKE_BINARY_DIR}/build")

# Include directories
include_directories(${INCLUDE_DIR} /opt/homebrew/include)

# Gather source files
file(GLOB_RECURSE CXX_SRCS "${SRC_DIR}/*.cpp")
file(GLOB_RECURSE C_SRCS "${SRC_DIR}/*.c")

# Define the executable
add_executable(${PROJECT_NAME} ${CXX_SRCS} ${C_SRCS})

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}
)

# Enable parallel compilation (handled automatically by CMake depending on the generator)

# Clean target equivalent (automatically managed by CMake, but you can add custom commands if needed)
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${BUILD_DIR} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}
    COMMENT "Cleaning all generated files."
)

# Message for build completion
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build complete!"
)
