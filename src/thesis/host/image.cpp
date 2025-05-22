#include "thesis/pch.h"
#include "thesis/host/image.h"
#include "device/average_samples.cuh"
#include "thesis/cuda/buffer.h"
#include "thesis/utils/math.h"
#include "thesis/cuda/stream_handle.h"
#include "thesis/utils/io.h"

#include <cstddef>
#include <filesystem>

namespace thesis {
namespace host {

core::Result Image::average_host() {
    sample_buffer_.download();

    const auto* src = sample_buffer_.host();
    auto* dst = averaged_pixels_.host();

    for (size_t i = 0; i < pixel_count(); ++i) {
        auto acc = make_float3(0.0f);
        for (size_t s = 0; s < samples_per_pixel_; ++s)
            acc += src[s * pixel_count() + i];
        dst[i] = acc / static_cast<float>(samples_per_pixel_);
    }

    return {};
}

core::Result Image::average_device(const cuda::StreamHandle& stream) {
    const dim3 block(16, 16);
    const dim3 grid(math::ceil_div(width_, block.x), math::ceil_div(height_, block.y));

    average_samples_kernel<<<grid, block, 0, stream>>>(
        averaged_pixels_.device(),
        sample_buffer_.device(),
        width_,
        height_,
        samples_per_pixel_
    );

    stream.synchronize();
    averaged_pixels_.download();

    return {};
}

core::Result Image::average(const cuda::StreamHandle& stream) {
    // TODO(kacper): select experimentally
    constexpr size_t PIXEL_THRESHOLD = math::pow2(512);
    constexpr size_t SAMPLE_THRESHOLD = 8;

    return (pixel_count() <= PIXEL_THRESHOLD && samples_per_pixel_ <= SAMPLE_THRESHOLD)
        ? average_host() : average_device(stream);
}

core::Result Image::save(const std::filesystem::path& filename, const cuda::StreamHandle& stream) noexcept {
    TRY(average(stream));
    return io::saveExrImage(averaged_pixels_.host_view(), width_, height_, filename);
}

}  // namespace host
}  // namespace thesis