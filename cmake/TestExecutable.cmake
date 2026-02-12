# =========================
# Test Executable setup
# =========================

# === Test Executable Target ===
add_executable(test_runner)

# === Source Files ===
# Get all test sources
file(GLOB_RECURSE TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/test/*.cpp")

# Get all src sources except main.cpp (which has its own main function)
file(GLOB_RECURSE SRC_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/*.cpp")
list(FILTER SRC_SRCS EXCLUDE REGEX ".*main\\.cpp$")

target_sources(test_runner PRIVATE
    ${TEST_SRCS}
    ${SRC_SRCS}
    $<TARGET_OBJECTS:thirdparty_objects>  # Link in 3rd-party .c/.cpp files
)

# === Dependencies ===
add_dependencies(test_runner build_optixir)

# === Precompiled Headers ===
target_precompile_headers(test_runner PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${INCLUDE_DIR}/thesis/pch.h>
)

# === Linking ===
target_link_libraries(test_runner PRIVATE
    device
    CUDA::cuda_driver
    CUDA::cudart
    CUDA::curand
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:TBB::tbb>
)

# === Include Directories ===
target_include_directories(test_runner PRIVATE
    ${INCLUDE_DIR}            # Our project headers
    ${CMAKE_SOURCE_DIR}/test  # Test headers
)

target_include_directories(test_runner SYSTEM PRIVATE
    ${THIRD_PARTY_DIR}        # 3rd-party includes (suppress warnings)
)

# === Compile Definitions ===
file(TO_CMAKE_PATH "${OPTIXIR_OUTPUT}" OPTIXIR_PATH_CMAKE_STYLE)
target_compile_definitions(test_runner PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    OPTIXIR_PATH="${OPTIXIR_PATH_CMAKE_STYLE}"
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<PLATFORM_ID:Windows>>:_AMD64_>
    $<$<AND:$<C_COMPILER_ID:MSVC>,$<PLATFORM_ID:Windows>>:_AMD64_>
)

# === Compile Options ===
target_compile_options(test_runner PRIVATE
    # Debug Flags (MSVC)
    $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:MSVC>>:/Od /Zi /W4 /permissive- /Zc:__cplusplus /utf-8>

    # Release Flags (MSVC)
    $<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:MSVC>>:/O2 /DNDEBUG /W4 /permissive- /Zc:__cplusplus /utf-8>

    # Silence "padded due to alignment" warnings
    $<$<C_COMPILER_ID:MSVC>:/wd4324>
)

# === Output Directory ===
set_target_properties(test_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}"
)
