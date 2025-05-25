#pragma once

#include "thesis/host/utils/result.h"

#include <vector_types.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>

namespace thesis::io {

core::Result<std::string> readFileToString(const std::filesystem::path& filename) noexcept;

core::Result<> saveExrImage(std::span<const float3> framebuffer, size_t width, size_t height,
                            const std::filesystem::path& filename,
                            bool flip_vertical = true) noexcept;

}  // namespace thesis::io
