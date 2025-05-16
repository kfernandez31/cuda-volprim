#pragma once

#include "common.cuh"

#include "thesis/device/ray.h"

#include <vector_types.h>
#include <curand_kernel.h>
#include <optix.h>

#include <sutil/vec_math.h>

__device__ __forceinline__ float sample_uniform(curandState* state) {
    return static_cast<float>(curand(state)) / static_cast<float>(UINT_MAX);
}

__device__ __forceinline__ float2 sample_random_2d(curandState* state, float2 offset=make_float2(0.5f)) {
    auto u = make_float2(sample_uniform(state), sample_uniform(state));
    return u - offset;
}

__forceinline__ __device__ thesis::device::Ray compute_jittered_ray(float2 jitter, uint3 idx) {
    const auto pixel = make_float2(idx.x + jitter.x, idx.y + jitter.y);
    const auto origin = params.camera_.eye_;
    const auto direction = normalize(
        params.camera_.pixel00_ +
        pixel.x * params.camera_.pixel_du_ +
        pixel.y * params.camera_.pixel_dv_ -
        origin
    );
    return thesis::device::Ray::spawn(origin, direction);
}
