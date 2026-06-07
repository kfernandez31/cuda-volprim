#include "kernels/wavefront_finalize.h"

#include "core/constants.cuh"

#include "thesis/common/utils/math.h"

#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace device {
namespace kernels {

namespace math = ::thesis::common::math;

constexpr size_t MAX_GRID_BLOCKS = 1024;
constexpr size_t BLOCK_SIZE = 256;

// One thread per pixel. Folds this batch's `samples_in_batch` per-(pixel,sample) radiances into
// the pixel's Welford running mean / M2. This is byte-for-byte the megakernel's per-sample
// accumulation (raygen.cuh lines ~263-316): same firefly clamp → non-finite rejection → Welford
// order, same n = prev_count + sample_in_batch + 1 weighting, same AOV running mean — just reading
// the radiance from RayState instead of computing it inline. Bit-identical given identical
// per-sample radiances.
static __global__ void wavefront_finalize_kernel(float4* out_mean, float4* out_variance,
                                                 uint32_t* sample_counts, float4* albedo_aov,
                                                 float4* normal_aov,
                                                 const params::RayState* __restrict__ ray_states,
                                                 size_t num_pixels, uint32_t samples_in_batch,
                                                 float firefly_clamp) {
    const size_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    const size_t stride = blockDim.x * gridDim.x;

    for (size_t pixel = tid; pixel < num_pixels; pixel += stride) {
        const uint32_t prev_count = sample_counts[pixel];
        auto mean = make_float3(out_mean[pixel]);

        auto M2 = make_float3(0.0f);
        if constexpr (consts::ENABLE_ADAPTIVE_SAMPLING) {
            M2 = make_float3(out_variance[pixel]);
        }

        const bool has_aov = (albedo_aov != nullptr);
        auto aov_albedo = make_float3(0.0f);
        auto aov_normal = make_float3(0.0f);
        if (has_aov) {
            aov_albedo = make_float3(albedo_aov[pixel]);
            aov_normal = make_float3(normal_aov[pixel]);
        }

        for (uint32_t s = 0; s < samples_in_batch; ++s) {
            const size_t ray_idx = static_cast<size_t>(s) * num_pixels + pixel;
            const auto& rs = ray_states[ray_idx];
            auto radiance = rs.radiance_;

            // Firefly clamp (off when 0) then non-finite rejection — same order as the megakernel.
            if (firefly_clamp > 0.0f) {
                const float lum = math::dot(radiance, make_float3(0.2126f, 0.7152f, 0.0722f));
                if (lum > firefly_clamp) {
                    radiance *= firefly_clamp * math::rcp(lum);
                }
            }
            if (!isfinite(math::sum(radiance))) {
                radiance = make_float3(0.0f);
            }

            const auto n_inv = math::rcp(static_cast<float>(prev_count + s + 1));
            const auto delta1 = radiance - mean;
            mean += delta1 * n_inv;
            if constexpr (consts::ENABLE_ADAPTIVE_SAMPLING) {
                const auto delta2 = radiance - mean;
                M2 += delta1 * delta2;
            }

            if (has_aov) {
                aov_albedo += (rs.aov_albedo_ - aov_albedo) * n_inv;
                aov_normal += (rs.aov_normal_ - aov_normal) * n_inv;
            }
        }

        if constexpr (consts::ENABLE_ADAPTIVE_SAMPLING) {
            out_variance[pixel] = make_float4(M2);
        }
        out_mean[pixel] = make_float4(mean);
        sample_counts[pixel] = prev_count + samples_in_batch;
        if (has_aov) {
            albedo_aov[pixel] = make_float4(aov_albedo);
            normal_aov[pixel] = make_float4(aov_normal);
        }
    }
}

void launch_wavefront_finalize_kernel(float4* mean, float4* variance, uint32_t* sample_counts,
                                      float4* albedo_aov, float4* normal_aov,
                                      const params::RayState* ray_states, size_t num_pixels,
                                      uint32_t samples_in_batch, float firefly_clamp,
                                      cudaStream_t stream) {
    const size_t num_blocks =
        common::math::min(common::math::ceil_div(num_pixels, BLOCK_SIZE), MAX_GRID_BLOCKS);

    wavefront_finalize_kernel<<<num_blocks, BLOCK_SIZE, 0, stream>>>(
        mean, variance, sample_counts, albedo_aov, normal_aov, ray_states, num_pixels,
        samples_in_batch, firefly_clamp);
}

}  // namespace kernels
}  // namespace device
}  // namespace thesis
