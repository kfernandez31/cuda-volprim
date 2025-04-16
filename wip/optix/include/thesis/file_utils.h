#pragma once

// #ifdef __cplusplus // TODO(kacper): reenable this

#include <vector_types.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace thesis {

std::optional<std::string> readFileToString(const std::string& filename);

void saveExrImage(const std::vector<float3>& framebuffer, size_t width, size_t height, const std::string& filename, bool flip_vertical = true);

} // namespace thesis

// #endif // __cplusplus
