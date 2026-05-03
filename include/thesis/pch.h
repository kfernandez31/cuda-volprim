#pragma once

// Prevent Windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

// C / C++ Standard Library
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <ios>
#include <math.h>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>
#include <vector_types.h>

// OptiX
#include <optix.h>
#include <optix_stubs.h>
#include <optix_types.h>

// 3rd Party — heavy single-use headers (CLI11, tinyexr) are pulled in by their
// one consumer (config.cpp, exr.cpp) instead of paying their parse cost in
// every translation unit.
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
