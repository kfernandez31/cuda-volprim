# =========================
# CUDA Device Library Setup
# =========================

# Find all CUDA source files (*.cu) in the device directory (recursively)
file(GLOB_RECURSE DEVICE_CU_SRCS CONFIGURE_DEPENDS "${DEVICE_DIR}/**/*.cu")

# Create a static library 'device' from the CUDA sources
add_library(device STATIC ${DEVICE_CU_SRCS})

# Set include directories for this target
target_include_directories(device PUBLIC
    ${DEVICE_DIR}
    ${INCLUDE_DIR}
    ${THIRD_PARTY_DIR}
)

# Set device target properties
# - CUDA_ARCHITECTURES: Target GPU architecture (e.g., 75 for Turing)
# - CUDA_SEPARABLE_COMPILATION: Enables separate compilation for CUDA
set_target_properties(device PROPERTIES
    CUDA_ARCHITECTURES ${CUDA_ARCH}
    CUDA_SEPARABLE_COMPILATION ON
)

# Enable numerical guards only for Debug/RelWithDebInfo builds
# Adds 5-10% overhead but catches numerical issues early
# Production (Release) builds should disable this for maximum performance
if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
    message(STATUS "Enabling THESIS_ENABLE_NUMERICAL_GUARDS for ${CMAKE_BUILD_TYPE} build")
    target_compile_definitions(device PUBLIC THESIS_ENABLE_NUMERICAL_GUARDS)
else()
    message(STATUS "Disabling THESIS_ENABLE_NUMERICAL_GUARDS for ${CMAKE_BUILD_TYPE} build (production)")
endif()

# Link CUDA libraries (device-level dependencies)
target_link_libraries(device PRIVATE
    CUDA::cuda_driver
    CUDA::cudart
    CUDA::curand
)

# Add OptiX library directory for linking (needed for OptiX runtime components)
link_directories("${OPTIX_ROOT}/lib64")
