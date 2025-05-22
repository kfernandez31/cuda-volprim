#pragma once

#include "thesis/device/pch.h"

// C / C++ Standard Library (host-only)
#include <array>
#include <cerrno>
#include <cstdlib>
#include <expected>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <memory>
#include <span>
#include <unordered_map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// CUDA (host-side only)
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>

// OptiX (host-side only)
#include <optix_host.h>
#include <optix_stubs.h>
#include <optix_types.h>

// 3rd Party (host-side only)
#include <CLI11/CLI11.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/optimum_pow.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stb/stb_image.h>
#include <tinyexr/tinyexr.h>