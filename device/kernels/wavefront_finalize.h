#pragma once

#include "thesis/device/params/ray_state.h"

#include <vector_types.h>

#include <cstddef>
#include <cstdint>

#include <cuda_runtime.h>

namespace thesis {
namespace device {
namespace kernels {

// Wavefront finalize (WAVEFRONT_PLAN.md Phase 1). After the host bounce loop completes, each
// RayState holds one sample's final radiance (and bounce-0 AOV). This kernel folds the batch's
// `samples_in_batch` samples per pixel into the per-pixel Welford running mean / M2 — exactly the
// per-sample accumulation the megakernel did inline, relocated so the bounce kernels stay pure.
//
// `variance` and the two AOV pointers may be null (adaptive / denoise off), matching the device
// Image contract. `firefly_clamp` mirrors RenderParams::firefly_clamp_luminance_ (0 = off).
extern "C" void launch_wavefront_finalize_kernel(
    float4* mean, float4* variance, uint32_t* sample_counts, float4* albedo_aov, float4* normal_aov,
    const params::RayState* ray_states, size_t num_pixels, uint32_t samples_in_batch,
    float firefly_clamp, cudaStream_t stream);

}  // namespace kernels
}  // namespace device
}  // namespace thesis
