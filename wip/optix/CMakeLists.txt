# cmake --build build --config Release
# cmake --build build -G "Visual Studio 17 2022" -A x64 --config Release

cmake_minimum_required(VERSION 3.20)
project(thesis LANGUAGES C CXX CUDA)

# === Configuration ===
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Output folders
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# === Include headers ===
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# === Source files ===
file(GLOB_RECURSE CXX_SRCS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE C_SRCS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

# Path to OptiX SDK (adjust if different)
set(OPTIX_ROOT "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0")
include_directories("${OPTIX_ROOT}/include")

# Expose header for CUDA compiler explicitly
set(OPTIX_INCLUDE_DIR "${OPTIX_ROOT}/include")

# === Device code ===
set(PTX_OUTPUT ${CMAKE_BINARY_DIR}/device_program.ptx)
set(DEVICE_SRC ${CMAKE_SOURCE_DIR}/device/device_program.cu)

add_custom_command(
    OUTPUT ${PTX_OUTPUT}
    COMMAND "${CMAKE_CUDA_COMPILER}"
        -I"${OPTIX_INCLUDE_DIR}"
        -ptx "${DEVICE_SRC}"
        -o "${PTX_OUTPUT}"
    DEPENDS ${DEVICE_SRC}
    COMMENT "Compiling PTX: ${DEVICE_SRC} -> ${PTX_OUTPUT}"
)

# === Add executable ===
add_executable(thesis
    ${C_SRCS}
    ${CXX_SRCS}
    ${PTX_OUTPUT}
)

# === CUDA & OptiX linking ===
# find_package(CUDA REQUIRED)
find_package(CUDAToolkit REQUIRED)

# Set CUDA architecture (you can adjust this)
set_target_properties(thesis PROPERTIES
    CUDA_ARCHITECTURES 75
)

# On Windows, add the OptiX SDK manually if not system-installed
# Replace this with the actual path if needed
target_include_directories(thesis PRIVATE "${OPTIX_ROOT}/include")

link_directories("${OPTIX_ROOT}/lib64")

target_link_libraries(thesis
    PRIVATE
        # optix.9.0.0
        # optix
        cuda
        cudart
        CUDA::cuda_driver
)

# Define macro so main.cpp knows where to find the PTX at runtime
target_compile_definitions(thesis PRIVATE
    PTX_PATH="${PTX_OUTPUT}"
    OPTIX_USE_HOST_API
)

message(STATUS "CUDA compiler: ${CMAKE_CUDA_COMPILER}")
message(STATUS "OptiX include: ${OPTIX_INCLUDE_DIR}")

# === Precompiled header (optional) ===
# target_precompile_headers(thesis PRIVATE include/thesis/pch.h)