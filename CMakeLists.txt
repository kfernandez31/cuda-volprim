# =========================
# Top-Level Project Configuration
# =========================

cmake_minimum_required(VERSION 3.20)
project(thesis LANGUAGES C CXX CUDA)

# Add clang-tidy and cppcheck support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find and include all modular CMake files
file(GLOB_RECURSE cmake_modules "${CMAKE_SOURCE_DIR}/cmake/*.cmake")
foreach(module ${cmake_modules})
    include(${module})
endforeach()

# Set Ninja as default generator if not specified
if(NOT CMAKE_GENERATOR)
    message(STATUS "Defaulting to Ninja generator")
    set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)
endif()


