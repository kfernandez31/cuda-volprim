# =========================
# PTX Compilation Logic
# =========================

# === PTX Compilation ===
set(DEVICE_ENTRY "${DEVICE_DIR}/device_program.cu")
set(PTX_OUTPUT   "${CMAKE_BINARY_DIR}/device_program.ptx")

add_custom_command(
    OUTPUT ${PTX_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling PTX: ${DEVICE_ENTRY} -> ${PTX_OUTPUT}"
    COMMAND "${CMAKE_CUDA_COMPILER}"
        -I "${OPTIX_ROOT}/include"
        -I "${INCLUDE_DIR}"
        -I "${DEVICE_DIR}"
        -I "${THIRD_PARTY_DIR}"
        ${NVCC_STD_FLAG}
        --ptx
        --expt-relaxed-constexpr
        --expt-extended-lambda
        -DGLM_ENABLE_EXPERIMENTAL
        -O3
        -arch=sm_${CUDA_ARCH}
        -o "${PTX_OUTPUT}"
        -diag-suppress=20044
        -m64
        "${DEVICE_ENTRY}"
    DEPENDS ${DEVICE_ENTRY}
    COMMENT "Compiling PTX..."
)

add_custom_target(build_ptx DEPENDS ${PTX_OUTPUT})