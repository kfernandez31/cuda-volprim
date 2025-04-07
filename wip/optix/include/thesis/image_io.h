#pragma once

#include <vector_types.h>

#include <string>
#include <string_view>
#include <vector>

namespace thesis {

std::string read_ptx(std::string_view filename);

void save_exr_image(const std::vector<float3>& framebuffer, int width, int height, std::string_view filename, bool flip_vertical = true);

} // namespace thesis
