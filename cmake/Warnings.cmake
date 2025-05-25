# =========================
# Compiler Warnings
# =========================

target_compile_options(thesis PRIVATE

    # GCC / Clang (C++)
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU,Clang>>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wnull-dereference
        -Wdouble-promotion -Wformat=2
    >

    # GCC / Clang (C)
    $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU,Clang>>:
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -Wformat=2
    >

    # Debug Flags (GCC / Clang)
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang>>:-g -O0>
    $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:GNU,Clang>>:-g -O0>

    # Release Flags (GCC / Clang)
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-O3 -DNDEBUG>
    $<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:GNU,Clang>>:-O3 -DNDEBUG>

    # Debug Flags (MSVC)
    $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:MSVC>>:/Od /Zi /W4 /permissive- /Zc:__cplusplus /utf-8>

    # Release Flags (MSVC)
    $<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:MSVC>>:/O2 /DNDEBUG /W4 /permissive- /Zc:__cplusplus /utf-8>
)
