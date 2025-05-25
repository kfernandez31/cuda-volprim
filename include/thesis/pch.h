#pragma once

// C / C++ Standard Library
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <math.h>
#include <type_traits>

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

// 3rd Party
#include <CLI11/CLI11.hpp>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/optimum_pow.hpp>
#include <glm/gtx/transform.hpp>
#include <happly/happly.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stb/stb_image.h>
#include <sutil/vec_math.h>
#include <tinyexr/tinyexr.h>