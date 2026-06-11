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

# Add fast math compilation flag for optimized builds
# This enables faster but slightly less precise math operations
# - Trades IEEE compliance for 2-3x faster transcendentals
# - Safe for Monte Carlo path tracing (statistical noise tolerates minor precision loss)
# The THESIS_ENABLE_FAST_MATH definition (PUBLIC so consumers like test_runner
# see it through math.h) gates the explicit hardware-intrinsic call sites in
# include/thesis/common/utils/math.h. Coupled with --use_fast_math so a future
# precision-audit build can drop both together via a single CMake option.
if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    target_compile_options(device PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--use_fast_math>)
    target_compile_definitions(device PUBLIC THESIS_ENABLE_FAST_MATH)
    message(STATUS "Enabling fast math for ${CMAKE_BUILD_TYPE} build")
else()
    message(STATUS "Fast math disabled for ${CMAKE_BUILD_TYPE} build (preserving precision for debugging)")
endif()

# Tessellated-icosphere GAS A/B (Chapter 6 G8): swap the analytic OptiX built-in sphere
# for a triangle-mesh icosphere, to benchmark our analytic choice against the reference's
# tessellated approach. OFF = analytic sphere (production default). THESIS_ICOSPHERE_N is
# the subdivision level (0 → 20 tris/12 verts … 3 → 1280 tris/642 verts). Declared here so
# the variable is visible to the later OptiX-IR / executable cmake includes. The define is
# PUBLIC on `device` so host consumers (renderer.cpp, gas.h) see it through the usual
# usage-requirement propagation, exactly like THESIS_ENABLE_FAST_MATH. The OptiX-IR build
# (device_program.cu, compiled by a separate custom command) needs the toggle injected
# explicitly via ${THESIS_ICOSPHERE_DEF} — see cmake/OptiX-IR.cmake.
option(THESIS_ICOSPHERE "Use a tessellated icosphere triangle GAS instead of the analytic built-in sphere" OFF)
set(THESIS_ICOSPHERE_N 3 CACHE STRING "Icosphere subdivision level N (0..3; 0=12 verts, 3=642 verts)")
if(THESIS_ICOSPHERE)
    message(STATUS "THESIS_ICOSPHERE=ON — tessellated icosphere GAS (subdivision N=${THESIS_ICOSPHERE_N})")
    target_compile_definitions(device PUBLIC THESIS_ICOSPHERE THESIS_ICOSPHERE_N=${THESIS_ICOSPHERE_N})
    set(THESIS_ICOSPHERE_DEF -DTHESIS_ICOSPHERE)  # OptiX-IR (device_program.cu) needs only the toggle
else()
    set(THESIS_ICOSPHERE_DEF "")
endif()

# Link CUDA libraries (device-level dependencies)
target_link_libraries(device PRIVATE
    CUDA::cuda_driver
    CUDA::cudart
    CUDA::curand
)

# Add OptiX library directory for linking (needed for OptiX runtime components)
link_directories("${OPTIX_ROOT}/lib64")
