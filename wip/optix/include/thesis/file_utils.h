#pragma once

#include <vector_types.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace thesis {

std::optional<std::string> read_file_to_string(std::string_view filename);

void save_exr_image(const std::vector<float3>& framebuffer, int width, int height, const std::string& filename, bool flip_vertical = true);

} // namespace thesis
