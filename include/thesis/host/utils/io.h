#pragma once

#include "thesis/host/params/primitive.h"
#include "thesis/host/utils/result.h"

#include <vector_types.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace thesis::host::utils::io {

[[nodiscard]] Result<std::vector<std::byte>> readFileToBytes(
    const std::filesystem::path& filename) noexcept;

[[nodiscard]] Result<> saveExrImage(std::span<const float3> framebuffer, size_t width,
                                    size_t height, const std::filesystem::path& filename,
                                    bool flip_vertical = true) noexcept;

using HDRImagePtr = std::unique_ptr<float, decltype(&stbi_image_free)>;
[[nodiscard]] Result<HDRImagePtr> loadHDRImage(const std::filesystem::path& filename, size_t& width,
                                               size_t& height, size_t& channels);

[[nodiscard]] Result<std::vector<params::Primitive>> loadPrimitives(
    const std::filesystem::path& filename);

}  // namespace thesis::host::utils::io
