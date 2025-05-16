#include "thesis/optix/launch_params.h"
#include "thesis/device/vector.h"
#include "thesis/device/set.h"
#include "thesis/utils/math.h"
#include "thesis/utils/vec_math.h"

#include <optix.h>

#include "common.cuh"
#include "random.cuh"
#include "trace.cuh"

constexpr auto MAX_HITS = 64u;
constexpr auto EPSILON = 1e-8f; // TODO(kacper) : toggle

using namespace thesis::device;

__forceinline__ __device__ float sample_distance(float sample, float sigma_t) {
    // Inverts the CDF of transmittance T(t) = exp(-sigma_t * t)
    // CDF = 1 - exp(-sigma_t * t) => t = -log(1 - sample) / sigma_t
    return -logf(fmaxf(1.0f - sample, 1e-6f)) / sigma_t;
}

__forceinline__ __device__ float3 sample_phase(const float3& wi, float2 sample) {
    // Isotropic phase function: uniform over sphere
    float z = 1.0f - 2.0f * sample.x;
    float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    float phi = 2.0f * math::PI_F * sample.y;
    return make_float3(r * cosf(phi), r * sinf(phi), z);
}

__device__ float3 sample_henyey_greenstein(float3 wi, float2 sample, float g) {
    float cos_theta;
    if (fabsf(g) < 1e-3f) {
        cos_theta = 1.0f - 2.0f * sample.x;
    } else {
        float sqr = (1.0f - math::pow2(g)) / (1.0f - g + 2.0f * g * sample.x);
        cos_theta = (1.0f + math::pow2(g) - math::pow2(sqr)) / (2.0f * g);
        cos_theta = math::clamp(cos_theta, -1.0f, 1.0f); // Clamp to avoid NaNs
    }

    auto sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta * cos_theta));
    auto phi = 2.0f * math::PI_F * sample.y;
    auto local = make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);

    // Build orthonormal basis around wi
    auto w = normalize(wi);
    auto u = normalize(cross(fabsf(w.z) > 0.99f ? make_float3(1,0,0) : make_float3(0,0,1), w));
    auto v = cross(w, u);

    return normalize(local.x * u + local.y * v + local.z * w);
}

__device__ float sample_distance_numerical(
    const Ray& ray,
    float t_min,
    float t_max,
    float sample,
    Set<unsigned int, MAX_HITS>& prim_indices
) {
    constexpr int MAX_ITER = 8;
    constexpr float EPS = 1e-4f;

    const float target_tau = -logf(fmaxf(1.0f - sample, 1e-6f));
    float t_lower = t_min;
    float t_upper = t_max;
    float t = 0.5f * (t_lower + t_upper);

    for (int iter = 0; iter < MAX_ITER; ++iter) {
        float tau = optical_depth_accumulated(ray, t, prim_indices);
        float d_tau = sigma_t_at(ray, t, prim_indices);

        float error = tau - target_tau;

        if (fabsf(error) < EPS)
            return t;

        // Newton-Raphson step
        float t_next = t - error / fmaxf(d_tau, 1e-4f);

        // Clamp and fallback to bisection
        if (t_next < t_lower || t_next > t_upper || isnan(t_next)) {
            t_next = 0.5f * (t_lower + t_upper);
        }

        if (error > 0.0f)
            t_upper = t;
        else
            t_lower = t;

        t = t_next;
    }

    return t;
}





__forceinline__ __device__ float3 integrate_primitives(const Ray& ray, float2 t_range, Set<unsigned int, MAX_HITS>& active_prim_indices) {
    auto result = make_float3(0.0f);
    for (auto idx : active_prim_indices) {
        const auto& prim = params.primitives_[idx];
        result += prim.density_integral(ray, t_range);
    }
    return result;
}

__device__ float3 compute_optical_depth_along_ray(const Ray& ray) {
    auto acc_optical_depth = make_float3(0.0f);
    auto t_old = 0.0f;
    /*
    Set<unsigned int, MAX_HITS> active_prim_indices;

    for (size_t hit = 0; hit < MAX_HITS; ++hit) {
        unsigned int t_raw, prim_idx, is_entry;
        trace(ray, t_old + EPSILON, INF_F, t_raw, prim_idx, is_entry);

        const auto t_new = __uint_as_float(t_raw);
        if (t_new >= INF_F) {
            break;
        }

        acc_optical_depth += integrate_primitives(ray, {t_old, t_new}, active_prim_indices);

        if (is_entry) {
            active_prim_indices.insert(prim_idx);
            if (active_prim_indices.full()) {
                break;
            }
        } else {
            active_prim_indices.erase(prim_idx);
        }

        t_old = t_new;
    }

    // drain remaining primitives
    acc_optical_depth += integrate_primitives(ray, {t_old, INF_F}, active_prim_indices);
    */
    return acc_optical_depth;
}

extern "C" __global__ void __raygen__rg() {
    const auto idx = optixGetLaunchIndex();

    auto acc_color = make_float3(0.0f);
    for (size_t sample = 0; sample < params.num_samples_per_pixel_; ++sample) {
        const auto jitter = sample_random_2d(sample, idx);
        const auto ray = compute_jittered_ray(jitter, idx);
        auto optical_depth = compute_optical_depth_along_ray(ray);
        acc_color += expf(-optical_depth) * params.env_map_.sample(ray.direction_);
    }
    acc_color /= static_cast<float>(params.num_samples_per_pixel_);
    params.image_(idx.x, idx.y) = acc_color;
}
