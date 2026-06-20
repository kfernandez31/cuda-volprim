# =========================
# Optix-IR Compilation Logic
# =========================

# === Compilation ===
set(DEVICE_ENTRY "${DEVICE_DIR}/device_program.cu")
set(OPTIXIR_OUTPUT "${CMAKE_BINARY_DIR}/device_program.optixir")

# Opt-in: fast approximate erf (Abramowitz & Stegun 7.1.26) in optical_depth's hot path.
# OFF by default → exact erff (validation/Mitsuba-comparison path unaffected). ON trades a
# documented ~5e-7 erf accuracy for ~1.5% throughput (FINDINGS §8.21). Gates math::fast_erf.
option(THESIS_ENABLE_FAST_ERF "Use fast approximate erf in the device hot path (~1.5%, ~5e-7 error)" OFF)
if(THESIS_ENABLE_FAST_ERF)
    set(THESIS_FAST_ERF_DEF -DTHESIS_ENABLE_FAST_ERF)
    message(STATUS "THESIS_ENABLE_FAST_ERF=ON — device build uses approximate erf (~1.5% faster)")
else()
    set(THESIS_FAST_ERF_DEF "")
endif()

# Opt-in: OptiX Shader Execution Reordering (SER) in the path-tracing loop. Inserts one
# optixReorder per bounce, keyed on the quantised scatter cell, to regroup divergent threads
# (Ada+ hardware; a no-op on Ampere/Turing). Pure scheduling — image is unchanged. Cross-arch
# probe for the §6 divergence autopsy; OFF by default so all 3090 builds are bit-identical.
option(THESIS_ENABLE_SER "Insert OptiX Shader Execution Reordering in the path loop (Ada+; no-op otherwise)" OFF)
if(THESIS_ENABLE_SER)
    set(THESIS_SER_DEF -DTHESIS_ENABLE_SER)
    message(STATUS "THESIS_ENABLE_SER=ON — device build inserts optixReorder per bounce (SER)")
else()
    set(THESIS_SER_DEF "")
endif()

# Free-form extra device defines for experiments (per-asset caps, SER hint variants). Pass a
# semicolon-separated CMake list of raw -D flags, e.g.
#   -DTHESIS_DEVICE_DEFS="-DTHESIS_HIT_BUFFER_CAPACITY=528;-DTHESIS_MAX_PRIMITIVES=32768"
set(THESIS_DEVICE_DEFS "" CACHE STRING "Extra raw -D flags for the device (optix-ir) compile")

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
        # Opt-in approximate erf (empty unless -DTHESIS_ENABLE_FAST_ERF=ON at configure).
        ${THESIS_FAST_ERF_DEF}
        # Opt-in SER reorder in the path loop (empty unless -DTHESIS_ENABLE_SER=ON at configure).
        ${THESIS_SER_DEF}
        # Free-form experiment defines (per-asset caps, SER hint variants); empty by default.
        ${THESIS_DEVICE_DEFS}
        # Tessellated-icosphere GAS A/B (empty unless -DTHESIS_ICOSPHERE=ON). Threads the
        # toggle into the any-hit front-face filter; defined in cmake/Device.cmake.
        ${THESIS_ICOSPHERE_DEF}
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