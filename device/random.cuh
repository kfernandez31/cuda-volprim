#pragma once

#include "thesis/device/ray.h"

#define DUPA
#ifdef DUPA
// TODO(kacper): remove dependency

#include "launch_params.cuh"
#include "thesis/device/launch_params.h"

#endif // DUPA

#include <vector_types.h>
#include <curand_kernel.h>
#include <optix.h>
#include <sutil/vec_math.h>

namespace thesis {
namespace device {
namespace random {

__device__ __forceinline__ float sample_uniform(curandState* state) {
    return static_cast<float>(curand(state)) / static_cast<float>(UINT_MAX);
}

__device__ __forceinline__ float2 sample_uniform_2d(curandState* state, float2 offset={}) {
    auto u = make_float2(sample_uniform(state), sample_uniform(state));
    return u - offset;
}

// TODO(kacper): split into compute_ray() (another header) and jitter_ray() (here)
// the jittered ray's direction must still unit
__forceinline__ __device__ Ray compute_jittered_ray(float2 jitter, uint3 idx) {
    const auto pixel = make_float2(idx.x + jitter.x, idx.y + jitter.y);
    const auto origin = params.camera_.eye_;
    const auto direction =
        params.camera_.pixel00_ +
        pixel.x * params.camera_.pixel_du_ +
        pixel.y * params.camera_.pixel_dv_ -
        origin;
    return Ray::spawn(origin, direction);
}

} // namespace random
} // namespace device
} // namespace thesis
