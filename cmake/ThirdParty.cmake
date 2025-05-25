# =========================
# Third-Party Object Libraries
# =========================

# Gather third-party sources
file(GLOB_RECURSE THIRD_PARTY_SOURCES CONFIGURE_DEPENDS
    "${THIRD_PARTY_DIR}/stb/*.c"
    "${THIRD_PARTY_DIR}/tinyexr/*.c"
    "${THIRD_PARTY_DIR}/tinyexr/*.cpp"
)

# Create an object library for third-party sources
add_library(thirdparty_objects OBJECT ${THIRD_PARTY_SOURCES})

# Suppress warnings for third-party sources
if (MSVC)
    set(WARNING_SUPPRESS_FLAG "/w")
else()
    set(WARNING_SUPPRESS_FLAG "-w")
endif()
set_source_files_properties(${THIRD_PARTY_SOURCES} PROPERTIES COMPILE_OPTIONS "${WARNING_SUPPRESS_FLAG}")

# Mark includes as SYSTEM to suppress header warnings
target_include_directories(thirdparty_objects SYSTEM INTERFACE ${THIRD_PARTY_DIR})

