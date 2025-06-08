#include "thesis/host/params/image.h"

#include "thesis/pch.h"

#include "kernels/average_samples.h"
#include "thesis/common/utils/math.h"
#include "thesis/host/cuda/buffer.h"
#include "thesis/host/cuda/stream_handle.h"
#include "thesis/host/utils/io.h"

#include <cstddef>
#include <filesystem>
#include <sutil/vec_math.h>

namespace thesis::host::params {

void Image::average(const cuda::StreamHandle& stream) {
    device::launch_average_samples_kernel(averaged_pixels_.device(), sample_buffer_.device(),
                                          width_, height_, num_samples_per_pixel_, stream.get());
    stream.synchronize();
    averaged_pixels_.download();
}

core::Result<> Image::save(const std::filesystem::path& filename,
                           const cuda::StreamHandle& stream) noexcept {
    average(stream);
    return io::saveExrImage(averaged_pixels_.host_view(), width_, height_, filename);
}

} // namespace thesis::host::params
