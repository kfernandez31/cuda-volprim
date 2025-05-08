#pragma once

// C / C++ Standard Library
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <expected>
#include <string_view>
#include <utility>
#include <vector>
#include <memory>
#include <type_traits>
#include <concepts>

// CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <vector_types.h>
#include <driver_types.h>

// OptiX
#include <optix.h>
#include <optix_host.h>
#include <optix_stubs.h>
#include <optix_types.h>

// 3rd Party
#include <CLI11/CLI11.hpp>
#include <glm/glm.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stb/stb_image.h>
#include <sutil/vec_math.h> // TODO(kacper): I think I only need operators and make_* functions from here
#include <tinyexr/tinyexr.h>
