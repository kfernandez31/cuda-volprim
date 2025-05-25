# =========================
# Main Executable setup
# =========================

# === Main Executable Target ===
add_executable(thesis)

# === Source Files ===
file(GLOB_RECURSE C_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/*.c")
file(GLOB_RECURSE CXX_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/*.cpp")

target_sources(thesis PRIVATE
    ${C_SRCS}
    ${CXX_SRCS}
    $<TARGET_OBJECTS:thirdparty_objects>  # Link in 3rd-party .c/.cpp files
)

# === Dependencies ===
add_dependencies(thesis build_ptx)

# === Precompiled Headers ===
target_precompile_headers(thesis PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${INCLUDE_DIR}/thesis/pch.h>
)

# === Linking ===
target_link_libraries(thesis PRIVATE
    device
    CUDA::cuda_driver
    CUDA::cudart
    CUDA::curand
)

# === Include Directories ===
target_include_directories(thesis PRIVATE
    ${INCLUDE_DIR}            # Our project headers
    SYSTEM ${THIRD_PARTY_DIR} # 3rd-party includes (suppress warnings)
)

# === Compile Definitions ===
file(TO_CMAKE_PATH "${PTX_OUTPUT}" PTX_PATH_CMAKE_STYLE)
target_compile_definitions(thesis PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    PTX_PATH="${PTX_PATH_CMAKE_STYLE}"
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<PLATFORM_ID:Windows>>:_AMD64_>
    $<$<AND:$<C_COMPILER_ID:MSVC>,$<PLATFORM_ID:Windows>>:_AMD64_>
)

# === MSVC-Specific Warning Suppressions ===
if (MSVC)
    target_compile_options(thesis PRIVATE /wd4324)  # Silence "padded due to alignment" warnings
endif()
