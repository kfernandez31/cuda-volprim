#pragma once

#include "common.cuh"

#include "thesis/device/ray.h"

#include <vector_types.h>

#include <sutil/vec_math.h>

// TODO(kacper): opt for another approach
__forceinline__ __device__ float2 sample_random_2d(const uint3& idx, int sample_index) {
    auto seed = idx.x * 73856093u ^ idx.y * 19349663u ^ sample_index * 83492791u;

    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    const auto x = (static_cast<float>((seed >> 0) & 0xFFFFu) / 65536.0f) - 0.5f;
    const auto y = (static_cast<float>((seed >> 16) & 0xFFFFu) / 65536.0f) - 0.5f;
    return {x, y};
}

__forceinline__ __device__ thesis::device::Ray compute_jittered_ray(const uint3& idx, const float2& jitter) {
    const auto pixel = make_float2(idx.x + jitter.x, idx.y + jitter.y);
    const auto origin = params.camera_.eye_;
    const auto direction = normalize(
        params.camera_.pixel00_ +
        pixel.x * params.camera_.pixel_du_ +
        pixel.y * params.camera_.pixel_dv_ -
        origin
    );
    return {origin, direction};
}
