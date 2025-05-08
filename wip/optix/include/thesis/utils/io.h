#pragma once

#include "thesis/utils/result.h"

#include <vector_types.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>

namespace thesis::io {

Result<std::string> readFileToString(const std::filesystem::path& filename);

Result<Unit> saveExrImage(std::span<const float3> framebuffer, size_t width, size_t height,
                          const std::filesystem::path& filename, bool flip_vertical = true);

}  // namespace thesis::io
