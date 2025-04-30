#pragma once

#ifdef __cplusplus

#include <vector_types.h>

#include <cstddef>
#include <optional>
#include <vector>
#include <span>
#include <string>
#include <string_view>

namespace thesis::io {

std::optional<std::string> readFileToString(std::string_view filename);

void saveExrImage(std::span<const float3> framebuffer, size_t width, size_t height, 
    std::string_view filename, bool flip_vertical = true);

}  // namespace thesis::io

#endif  // __cplusplus
