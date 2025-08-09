# =========================
# Core Build Settings
# =========================

# === ccache Support ===
find_program(CCACHE NAMES ccache)
if(CCACHE)
    message(STATUS "Using ccache: ${CCACHE}")
    foreach(lang C CXX CUDA)
        set(CMAKE_${lang}_COMPILER_LAUNCHER "${CCACHE}")
    endforeach()
endif()

# === Language Standards ===
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# === CUDA Architecture & NVCC Standard Mapping ===
set(CUDA_ARCH "50" CACHE STRING "CUDA architecture to compile for") # Maxwell
if (CMAKE_CXX_STANDARD GREATER_EQUAL 23)
    set(NVCC_STD_FLAG "--std=c++20")  # Match CUDA's supported C++ standard
elseif (CMAKE_CXX_STANDARD STREQUAL "20")
    set(NVCC_STD_FLAG "--std=c++20")
elseif (CMAKE_CXX_STANDARD STREQUAL "17")
    set(NVCC_STD_FLAG "--std=c++17")
elseif (CMAKE_CXX_STANDARD STREQUAL "14")
    set(NVCC_STD_FLAG "--std=c++14")
else()
    message(FATAL_ERROR "Unsupported CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD} for NVCC")
endif()

# === Project Paths ===
set(SRC_DIR         "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR     "${CMAKE_SOURCE_DIR}/include")
set(DEVICE_DIR      "${CMAKE_SOURCE_DIR}/device")
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

# === OptiX SDK Root ===
if (WIN32)
    set(DEFAULT_OPTIX_ROOT "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0")
else()
    set(DEFAULT_OPTIX_ROOT "/home/prybicki/NVIDIA-OptiX-SDK-8.0.0-linux64-x86_64")
endif()
set(OPTIX_ROOT "${OPTIX_ROOT}" CACHE PATH "Path to OptiX SDK")
if(NOT OPTIX_ROOT)
    set(OPTIX_ROOT "${DEFAULT_OPTIX_ROOT}")
endif()

# === Output Directory ===
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# === CUDA Toolkit ===
find_package(CUDAToolkit REQUIRED)

# === Global Include Paths ===
# - Project headers
include_directories(${INCLUDE_DIR})
# - OptiX SDK headers (mark as system to suppress warnings)
include_directories(SYSTEM "${OPTIX_ROOT}/include")
