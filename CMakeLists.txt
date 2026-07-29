# =========================
# Top-Level Project Configuration
# =========================

cmake_minimum_required(VERSION 3.20)
project(cuda-volprim LANGUAGES C CXX CUDA)

# Add clang-tidy and cppcheck support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the modular CMake files in dependency order (explicit, not globbed: e.g. Warnings.cmake
# mutates the thesis/test_runner targets, OptiX-IR.cmake consumes vars set by Device.cmake, and
# the executables consume thirdparty_objects + OPTIXIR_OUTPUT — alphabetical glob order only worked
# by coincidence and breaks silently if a module is renamed).
include(${CMAKE_SOURCE_DIR}/cmake/Config.cmake)          # paths, std, CUDA arch, OptiX root
include(${CMAKE_SOURCE_DIR}/cmake/ThirdParty.cmake)      # thirdparty_objects
include(${CMAKE_SOURCE_DIR}/cmake/Device.cmake)          # device CUDA lib (+ THESIS_WAVEFRONT def)
include(${CMAKE_SOURCE_DIR}/cmake/OptiX-IR.cmake)        # device_program.optixir (uses Device vars)
include(${CMAKE_SOURCE_DIR}/cmake/MainExecutable.cmake)  # thesis executable
include(${CMAKE_SOURCE_DIR}/cmake/TestExecutable.cmake)  # test_runner executable
include(${CMAKE_SOURCE_DIR}/cmake/Warnings.cmake)        # mutates thesis/test_runner — must be last

# Set Ninja as default generator if not specified
if(NOT CMAKE_GENERATOR)
    message(STATUS "Defaulting to Ninja generator")
    set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)
endif()


