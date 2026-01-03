#pragma once

#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/params/primitive.h"
#include "thesis/host/utils/result.h"

#include <vector_types.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <future>
#include <span>
#include <vector>

namespace thesis::host::utils::io {

// Custom deleter for CUDA pinned memory
struct CudaPinnedDeleter {
    void operator()(float* ptr) const noexcept;
};

using HDRImagePtr = std::unique_ptr<float, CudaPinnedDeleter>;

struct HDRImageData {
    HDRImagePtr data;
    size_t width;
    size_t height;
    size_t channels;
};

// Async I/O operations (all file operations happen on background threads)
namespace async {

[[nodiscard]] std::future<Result<std::vector<std::byte>>> readFileToBytes(
    const std::filesystem::path& filename);

// Loads HDR image, allocates pinned memory for async CUDA transfers
[[nodiscard]] std::future<Result<HDRImageData>> loadHDR(const std::filesystem::path& filename);

// Loads primitives from PLY file
[[nodiscard]] std::future<Result<std::vector<params::Primitive>>> loadPrimitives(
    const std::filesystem::path& filename);

// Saves image to EXR file (extracts RGB from RGBA, W component ignored)
[[nodiscard]] std::future<Result<>> saveExr(cuda::AsyncBuffer<float4>&& buffer, size_t width,
                                            size_t height, const std::filesystem::path& filename,
                                            bool flip_vertical = true) noexcept;

}  // namespace async

}  // namespace thesis::host::utils::io
