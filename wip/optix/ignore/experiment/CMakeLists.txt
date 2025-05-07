cmake_minimum_required(VERSION 3.20)
project(thesis LANGUAGES CXX CUDA)

# Set C++ and CUDA standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Where is OptiX SDK and CUDA Toolkit
set(OPTIX_ROOT "C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0")
set(CUDA_TOOLKIT_ROOT_DIR "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4")

# Include OptiX and CUDA headers
include_directories(
    "${OPTIX_ROOT}/include"
    "${CUDA_TOOLKIT_ROOT_DIR}/include"
    "${CMAKE_SOURCE_DIR}"
)

# List all source files
set(SOURCES
    main.cpp
    # plus any additional .cpp files you may have
)

# Force main.cpp to be compiled as CUDA
set_source_files_properties(main.cpp PROPERTIES LANGUAGE CUDA)

# Create executable
add_executable(thesis ${SOURCES})

# Target properties for CUDA
set_target_properties(thesis PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
)

# Link CUDA libraries
find_package(CUDAToolkit REQUIRED)

target_link_libraries(thesis PRIVATE
    CUDA::cuda_driver
    CUDA::cudart
)

# Extra flags for MSVC to avoid ninja + nvcc confusion
if(MSVC)
    target_compile_options(thesis PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler="/MD /O2 /Ob2 /DNDEBUG /W4 /permissive- /Zc:__cplusplus /utf-8">
    )
endif()