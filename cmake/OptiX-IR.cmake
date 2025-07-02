# =========================
# Optix-IR Compilation Logic
# =========================

# === Compilation ===
set(DEVICE_ENTRY "${DEVICE_DIR}/device_program.cu")
set(OPTIXIR_OUTPUT "${CMAKE_BINARY_DIR}/device_program.optixir")

add_custom_command(
    OUTPUT ${OPTIXIR_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling OptiX-IR: ${DEVICE_ENTRY} -> ${OPTIXIR_OUTPUT}"
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
        -o "${OPTIXIR_OUTPUT}"
        -diag-suppress=20044
        -m64
        "${DEVICE_ENTRY}"
    DEPENDS ${DEVICE_ENTRY}
    COMMENT "Compiling OptiX-IR..."
)

add_custom_target(build_optixir DEPENDS ${OPTIXIR_OUTPUT})