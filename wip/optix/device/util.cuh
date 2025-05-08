#pragma once

#include <cuda_runtime.h>

#include <sutil/vec_math.h>

struct alignas(16) Ray {
    float3 origin_;
    float3 direction_;
};

// TODO(kacper): opt for another approach
__forceinline__ __device__ float2 sample_random_2d(const uint3& idx, int sample_index) {
    auto seed = idx.x * 73856093u ^ idx.y * 19349663u ^ sample_index * 83492791u;

    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    auto x = (float((seed >> 0) & 0xFFFFu) / 65536.0f) - 0.5f;
    auto y = (float((seed >> 16) & 0xFFFFu) / 65536.0f) - 0.5f;
    return make_float2(x, y);
}

__forceinline__ __device__ Ray compute_jittered_ray(
    const uint3& idx, const float2& jitter) {

    const float2 pixel = make_float2(idx.x + jitter.x, idx.y + jitter.y);
    const float3 origin = params.camera_.eye_;
    const float3 direction = normalize(
        params.camera_.pixel00_ +
        pixel.x * params.camera_.pixel_du_ +
        pixel.y * params.camera_.pixel_dv_ -
        origin
    );
    return Ray{origin, direction};
}