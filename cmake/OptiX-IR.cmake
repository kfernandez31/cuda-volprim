# =========================
# Optix-IR Compilation Logic
# =========================

# === Compilation ===
set(DEVICE_ENTRY "${DEVICE_DIR}/device_program.cu")
set(OPTIXIR_OUTPUT "${CMAKE_BINARY_DIR}/device_program.optixir")

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
        -O3
        -arch=sm_${CUDA_ARCH}
        # Fast math flags for FMA and aggressive FP optimizations
        -use_fast_math
        --fmad=true
        --ftz=true
        --prec-div=false
        --prec-sqrt=false
        # TODO: Experiment with --maxrregcount=N (e.g. 96 or 128) to limit per-thread
        # register usage. Lower values increase occupancy (more warps in flight) at the
        # cost of register spills to local memory. Profile with NSight Compute to find
        # the sweet spot — the large per-thread hit buffer (~8KB) likely already spills,
        # so the benefit may be limited until that structure is reduced.
        -o "${OPTIXIR_OUTPUT}"
        -diag-suppress=20044
        -m64
        "${DEVICE_ENTRY}"
    DEPENDS ${DEVICE_ENTRY}
    COMMENT "Compiling OptiX-IR..."
)

add_custom_target(build_optixir DEPENDS ${OPTIXIR_OUTPUT})