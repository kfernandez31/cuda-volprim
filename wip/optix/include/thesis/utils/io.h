#pragma once

#include <vector_types.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace thesis::io {

std::optional<std::string> readFileToString(std::string_view filename);

std::optional<std::pair<int, std::string>> saveExrImage(std::span<const float3> framebuffer, size_t width, size_t height,
                  std::string_view filename, bool flip_vertical = true);

}  // namespace thesis::io
