# =========================
# Optix-IR Compilation Logic
# =========================

# === Compilation ===
set(DEVICE_ENTRY "${DEVICE_DIR}/device_program.cu")
set(OPTIXIR_OUTPUT "${CMAKE_BINARY_DIR}/device_program.optixir")

# Track all device headers so changes trigger rebuild
file(GLOB_RECURSE DEVICE_HEADERS
    "${DEVICE_DIR}/*.cuh"
    "${INCLUDE_DIR}/thesis/device/*.h"
    "${INCLUDE_DIR}/thesis/common/*.h"
)

add_custom_command(
    OUTPUT ${OPTIXIR_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling OptiX-IR: ${DEVICE_ENTRY} to ${OPTIXIR_OUTPUT}"
    COMMAND "${CMAKE_CUDA_COMPILER}"
        -I "${OPTIX_ROOT}/include"
        -I "${INCLUDE_DIR}"
        -I "${DEVICE_DIR}"
        -I "${THIRD_PARTY_DIR}"
        ${NVCC_STD_FLAG}
        --optix-ir
        --relocatable-device-code=true
        --generate-line-info
        --expt-relaxed-constexpr
        --expt-extended-lambda
        -DGLM_ENABLE_EXPERIMENTAL
        # Couples with --use_fast_math below: gates the explicit hardware
        # intrinsics in include/thesis/common/utils/math.h. OptiX-IR is always
        # optimised, so always defined here. See cmake/Device.cmake for the
        # device-library equivalent (Release/RelWithDebInfo only).
        -DTHESIS_ENABLE_FAST_MATH
        -O3
        -arch=sm_${CUDA_ARCH}
        # Fast math flags for FMA and aggressive FP optimizations
        -use_fast_math
        --fmad=true
        --ftz=true
        --prec-div=false
        --prec-sqrt=false
        -o "${OPTIXIR_OUTPUT}"
        -diag-suppress=20044
        -m64
        "${DEVICE_ENTRY}"
    DEPENDS ${DEVICE_ENTRY} ${DEVICE_HEADERS}
    COMMENT "Compiling OptiX-IR..."
)

add_custom_target(build_optixir DEPENDS ${OPTIXIR_OUTPUT})