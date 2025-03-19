# Specify GCC compilers
set(CMAKE_C_COMPILER /opt/homebrew/bin/gcc-14)
set(CMAKE_CXX_COMPILER /opt/homebrew/bin/g++-14)

# Use GCC's standard library instead of macOS's default libc++
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")

# Point to GCC's system includes (instead of Apple SDKs)
set(CMAKE_SYSROOT "")

# Fix OpenMP issues for GCC
set(OpenMP_C_FLAGS "-fopenmp")
set(OpenMP_CXX_FLAGS "-fopenmp")
set(OpenMP_EXE_LINKER_FLAGS "-fopenmp")
