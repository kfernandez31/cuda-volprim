// #include "thesis/host/pch.h"
#include "thesis/host/params/image.h"

#include "thesis/device/kernels/core/average_samples.cuh"

#include "thesis/common/utils/math.h"
#include "thesis/host/cuda/buffer.h"
#include "thesis/host/cuda/stream_handle.h"
#include "thesis/host/utils/io.h"

#include <cstddef>
#include <filesystem>
#include <sutil/vec_math.h>

namespace thesis {
namespace host {

extern "C" void launch_average_samples_kernel(
    float3* out_img,
    const float3* in_buf,
    size_t width,
    size_t height,
    size_t num_samples_per_pixel,
    cudaStream_t stream
);

core::Result<> Image::average_host() {
    sample_buffer_.download();

    const auto* src = sample_buffer_.host();
    auto* dst = averaged_pixels_.host();

    for (size_t i = 0; i < pixel_count(); ++i) {
        auto acc = make_float3(0.0f);
        for (size_t s = 0; s < num_samples_per_pixel_; ++s)
            acc += src[s * pixel_count() + i];
        dst[i] = acc / static_cast<float>(num_samples_per_pixel_);
    }

    return {};
}

core::Result<> Image::average_device(const cuda::StreamHandle& stream) {
    launch_average_samples_kernel(averaged_pixels_.device(), sample_buffer_.device(), width_, height_, num_samples_per_pixel_, stream.get());

    stream.synchronize();
    averaged_pixels_.download();

    return {};
}

core::Result<> Image::average(const cuda::StreamHandle& stream) {
    // TODO(kacper): select experimentally
    constexpr size_t PIXEL_THRESHOLD = math::pow(512, 2u);
    constexpr size_t SAMPLE_THRESHOLD = 8;

    return (pixel_count() <= PIXEL_THRESHOLD && num_samples_per_pixel_ <= SAMPLE_THRESHOLD)
               ? average_host()
               : average_device(stream);
}

core::Result<> Image::save(const std::filesystem::path& filename,
                         const cuda::StreamHandle& stream) noexcept {
    TRY(average(stream));
    return io::saveExrImage(averaged_pixels_.host_view(), width_, height_, filename);
}

}  // namespace host
}  // namespace thesis