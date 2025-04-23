# ========================
# Build thesis (C++/CUDA/OptiX)
# ========================

cmake_minimum_required(VERSION 3.20)
project(thesis LANGUAGES C CXX CUDA)

# === Standards & Compiler Options ===
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Needed by Clang-Tidy and IWYU

# === Project Paths ===
set(SRC_DIR         "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR     "${CMAKE_SOURCE_DIR}/include")
set(DEVICE_DIR      "${CMAKE_SOURCE_DIR}/device")
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

# === OptiX SDK Root ===
if (WIN32)
    set(DEFAULT_OPTIX_ROOT "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0")
elseif(UNIX)
    set(DEFAULT_OPTIX_ROOT "/opt/optix")
endif()

set(OPTIX_ROOT "${OPTIX_ROOT}" CACHE PATH "Path to OptiX SDK")
if(NOT OPTIX_ROOT)
    set(OPTIX_ROOT "${DEFAULT_OPTIX_ROOT}")
endif()

# === Output Directory ===
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# === Own Includes ===
include_directories(
    ${INCLUDE_DIR}
)

# === Optix Includes (suppress warnings) ===
include_directories(SYSTEM
    "${OPTIX_ROOT}/include"
)

# ===  3rd-party Includes (suppress warnings) ===
file(GLOB THIRD_PARTY_SUBDIRS CONFIGURE_DEPENDS "${THIRD_PARTY_DIR}/*")
foreach(dir ${THIRD_PARTY_SUBDIRS})
    if(IS_DIRECTORY ${dir})
        include_directories(SYSTEM ${dir})
    endif()
endforeach()
include_directories(SYSTEM ${THIRD_PARTY_DIR})

# === Source Files (project)
file(GLOB_RECURSE C_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/*.c")
file(GLOB_RECURSE CXX_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/*.cpp")

# === Source Files (third-party, compiled with warnings disabled) ===
file(GLOB_RECURSE THIRD_PARTY_C_SRCS CONFIGURE_DEPENDS "${THIRD_PARTY_DIR}/*.c")
file(GLOB_RECURSE THIRD_PARTY_CXX_SRCS CONFIGURE_DEPENDS "${THIRD_PARTY_DIR}/*.cpp")

add_library(thirdparty_objects OBJECT
    ${THIRD_PARTY_C_SRCS}
    ${THIRD_PARTY_CXX_SRCS}
)

target_compile_options(thirdparty_objects PRIVATE
    # Fix MSVC warnings about missing exception unwind semantics
    $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
    $<$<C_COMPILER_ID:MSVC>:/EHsc>

    $<$<CXX_COMPILER_ID:GNU,Clang>:-w>
    $<$<C_COMPILER_ID:GNU,Clang>:-w>

    $<$<CXX_COMPILER_ID:MSVC>:/W0>
    $<$<C_COMPILER_ID:MSVC>:/W0>
)

# === CUDA Device Code (PTX) ===
set(DEVICE_SRC   "${DEVICE_DIR}/device_program.cu")
set(PTX_OUTPUT   "${CMAKE_BINARY_DIR}/device_program.ptx")

# === Target Settings ===
set(CUDA_ARCH "75" CACHE STRING "CUDA architecture to compile for")

add_custom_command(
    OUTPUT  ${PTX_OUTPUT}
    COMMAND "${CMAKE_CUDA_COMPILER}"
        -I"${OPTIX_ROOT}/include"
        --ptx
        --expt-relaxed-constexpr
        --expt-extended-lambda
        -O3
        -arch=sm_${CUDA_ARCH}
        -o "${PTX_OUTPUT}"
        -diag-suppress=20044
        "${DEVICE_SRC}"
    DEPENDS ${DEVICE_SRC}
    COMMENT "Compiling PTX: ${DEVICE_SRC} -> ${PTX_OUTPUT}"
)

# === Executable Target ===
add_executable(thesis
    ${C_SRCS}
    ${CXX_SRCS}
    ${PTX_OUTPUT}
)

# === Link 3rd-party object files ===
target_sources(thesis PRIVATE $<TARGET_OBJECTS:thirdparty_objects>)

# === Target Settings ===
set_target_properties(thesis PROPERTIES CUDA_ARCHITECTURES ${CUDA_ARCH})

# === Build-type and platform-specific flags ===
target_compile_options(thesis PRIVATE

    # Common warnings (C++)
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU,Clang>>:
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wno-sign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    >

    # Common warnings (C)
    $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU,Clang>>:
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wno-sign-conversion
        -Wformat=2
    >

    # Debug flags (C++ / GCC / Clang)
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU,Clang>>:
        -g
        -O0
    >

    # Debug flags (C / GCC / Clang)
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU,Clang>>:
        -g
        -O0
    >

    # Release flags (C++ / GCC / Clang)
    $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU,Clang>>:
        -O3
        -DNDEBUG
    >

    # Release flags (C / GCC / Clang)
    $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU,Clang>>:
        -O3
        -DNDEBUG
    >

    # Debug flags (MSVC C/C++)
    $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:MSVC>>:/Od /Zi /W4 /permissive- /Zc:__cplusplus /utf-8>

    # Release flags (MSVC C/C++)
    $<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:MSVC>>:/O2 /DNDEBUG /W4 /permissive- /Zc:__cplusplus /utf-8>
)

# === CUDA Toolkit ===
find_package(CUDAToolkit REQUIRED)

# === Link Directories and Libraries ===
if(WIN32)
    link_directories("${OPTIX_ROOT}/lib64")
elseif(UNIX)
    link_directories("${OPTIX_ROOT}/lib")
endif()

target_link_libraries(thesis PRIVATE
    CUDA::cuda_driver
    CUDA::cudart
)

# === Compile-Time Defines ===
file(TO_CMAKE_PATH "${PTX_OUTPUT}" PTX_PATH_CMAKE_STYLE)
target_compile_definitions(thesis PRIVATE
    PTX_PATH="${PTX_PATH_CMAKE_STYLE}"

    # Fix "No Target Architecture" issue from Windows SDK headers
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<PLATFORM_ID:Windows>>:_AMD64_>
    $<$<AND:$<C_COMPILER_ID:MSVC>,$<PLATFORM_ID:Windows>>:_AMD64_>
)

# === Static Analysis: Clang-Tidy ===
find_program(CLANG_TIDY NAMES clang-tidy)
if(CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")
    message(STATUS "Clang-Tidy enabled: ${CLANG_TIDY}")
endif()

# === Static Analysis: Cppcheck ===
find_program(CPPCHECK NAMES cppcheck)
if(CPPCHECK)
    set(CMAKE_CXX_CPPCHECK "${CPPCHECK};--enable=all;--inconclusive;--inline-suppr;--std=c++20;--quiet")
    message(STATUS "Cppcheck enabled: ${CPPCHECK}")
endif()

# === Formatting: Clang-Format ===
find_program(CLANG_FORMAT NAMES clang-format)
if (CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/include/*.h"
    )
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format"
    )
endif()

# === Generator Fallback ===
if(NOT CMAKE_GENERATOR)
    message(STATUS "Defaulting to Ninja generator")
    set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)
endif()