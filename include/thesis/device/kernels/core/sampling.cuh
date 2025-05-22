#pragma once

#include "thesis/device/kernels/launch_params.cuh"
#include "thesis/common/params/launch_params.h"
#include "thesis/device/utils/vector.h"
#include "thesis/device/utils/math.h"
#include "thesis/device/utils/optional.h"

#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/types.h"

#include <optix.h>
#include <math.h>
#include <sutil/vec_math.h>

#include "thesis/device/kernels/core/common.cuh"
#include "thesis/device/kernels/core/random.cuh"
#include "thesis/device/kernels/core/trace.cuh"

namespace thesis {
namespace device {

namespace consts {

constexpr auto MAX_HITS = 64u;
constexpr auto EPSILON = 1e-8f;
constexpr auto PHASE_VALUE = math::ONE_OVER_FOUR_PI_F; // 1 over unit sphere surface

} // namespace consts

// TODO(kacper): move somewhere perhaps

__forceinline__ __device__ float3 sample_phase(float3 /*wi*/, float2 sample) {
    // Isotropic phase function: uniform over sphere
    // Role:
    // Determines in which direction light scatters after the event.

    // Mechanism:
    // Draws a new direction from a phase function, which is a PDF over the unit sphere. Controls anisotropy of scattering.
    auto z = 1.0f - 2.0f * sample.x;
    auto r = sqrtf(fmaxf(0.0f, 1.0f - math::pow2(z)));
    auto phi = math::TWO_PI_F * sample.y;
    return make_float3(r * cosf(phi), r * sinf(phi), z); // direction, already unit
}

//  inverse CDF for τ
__device__ __forceinline__ float sample_target_optical_depth(float uniform_sample) {
    // Inverse transform sampling from exponential distribution:
    // PDF:   p(τ) = e^(-τ)
    // CDF:   F(τ) = 1 - e^(-τ)
    // Inverse CDF: τ = -ln(1 - ξ), ξ ∈ [0,1)

    // Clamp to avoid log(0), which would be infinite
    return -logf(fmaxf(1.0f - uniform_sample, 1e-6f));
}

__device__ float optical_depth_accumulated(
    const Ray& ray,
    float2 segment,
    utils::Set<uint, MAX_HITS>& prim_indices
) {
    AUTO tau = 0.0f;

    for (auto idx : prim_indices) {
        const auto& prim = params.primitives_[idx];
        tau += prim.optical_depth(ray, segment);
    }

    return tau;
}

// TODO(kacper) potential to optimize: don't invert, select random position on the segment which we call the ... and here the conversation with Jorge broke so I don't know what he meant
// bisection solver for τ(t) = χ
__device__ float sample_distance_bisection(
    const Ray& ray,
    float2 segment,
    float tau_needed,
    utils::Set<uint, MAX_HITS>& prim_indices
) {
    constexpr auto MAX_ITER = 24u;
    constexpr auto EPS = 1e-4f;

    auto t_lo = segment.x;
    auto t_hi = segment.y;

    for (size_t i = 0; i < MAX_ITER && (t_hi - t_lo) > EPS; ++i) {
        auto t_mid = 0.5f * (t_lo + t_hi);
        auto tau = optical_depth_accumulated(ray, {t_lo, t_mid}, prim_indices);

        if (tau >= tau_needed)
            t_hi = t_mid;
        else
            t_lo = t_mid;
    }

    return (t_hi - t_lo <= EPS) ? t_hi : 0.5f * (t_lo + t_hi);
}

__device__ float3 evaluate_albedo(float3 pos, utils::Set<uint, MAX_HITS>& prim_indices) {
    auto accum_albedo = make_float3(0.0f);
    auto accum_weight = 0.0f;

    for (auto idx : prim_indices) {
        const auto& prim = params.primitives_[idx];

        const auto sigma_t = prim.optical_depth_scale_;  // extinction coefficient
        const auto albedo = prim.albedo_;
        const auto pdf = prim.kernel_pdf(pos);           // density at pos

        const auto weight = sigma_t * pdf;

        accum_albedo += albedo * weight;
        accum_weight += weight;
    }

    // TODO(kacper): I believe the == 0.0f case is unreachable
    return (accum_weight > 0.0f) ? accum_albedo / accum_weight : make_float3(0.0f);
}

__device__ utils::Optional<ScatteringEvent<MAX_HITS>> sample_scattering_event(const Ray& ray, curandState* rng) {
    auto t_total = 0.0f;
    auto tau_cumulative = 0.0f;

    utils::Set<uint, MAX_HITS> active_prims;
    const auto tau_target = sample_target_optical_depth(random::sample_uniform(rng));

    for (size_t hit = 0; hit < MAX_HITS; ++hit) {
        uint t_raw, prim_idx, is_entry;
        trace_chray, {t_total + EPSILON, INF_F}, t_raw, prim_idx, is_entry);

        auto t_hit = __uint_as_float(t_raw);
        if (t_hit >= INF_F)
            break;

        const auto segment = make_float2(t_total, t_hit);
        const auto tau_segment = optical_depth_accumulated(ray, segment, active_prims);

        // scattering occurred
        // [                                t                      ]
        // ^- tau_cumulative & t_total      ^- tau_target & t      ^-tau_target + tau_segment & t_hit
        if (tau_cumulative + tau_segment >= tau_target) {
            auto tau_needed = tau_target - tau_cumulative;

            auto t = sample_distance_bisection(ray, segment, tau_needed, active_prims);
            auto pos = ray.at(t);
            auto rnd = random::sample_uniform_2d(rng);
            auto dir_out = sample_phase(-ray.direction_, rnd);

            return {t, pos, dir_out, active_prims};
        }

        // Update primitive state
        if (is_entry) {
            active_prims.insert(prim_idx);
            if (active_prims.full())
                break; // TODO(kacper): what to do here?
        } else {
            active_prims.erase(prim_idx);
        }

        t_total = t_hit;
        tau_cumulative += tau_segment;
    }

    // No scattering event found
    return {};
}

__device__ float3 compute_optical_depth_along_ray(const Ray& ray) {
    auto acc_optical_depth = make_float3(0.0f);
    auto t_old = 0.0f;

    utils::Set<uint, MAX_HITS> active_prims;

    for (size_t hit = 0; hit < MAX_HITS; ++hit) {
        uint t_raw, prim_idx, is_entry;
        trace_chray, t_old + EPSILON, INF_F, t_raw, prim_idx, is_entry);

        const auto t_new = __uint_as_float(t_raw);
        if (t_new >= INF_F) {
            break;
        }

        acc_optical_depth += integrate_primitives(ray, {t_old, t_new}, active_prims);

        if (is_entry) {
            active_prims.insert(prim_idx);
            if (active_prims.full()) {
                break; // TODO(kacper): what to do here?
            }
        } else {
            active_prims.erase(prim_idx);
        }

        t_old = t_new;
    }

    // drain remaining primitives
    acc_optical_depth += integrate_primitives(ray, {t_old, INF_F}, active_prims);

    return acc_optical_depth;
}


} // namespace device
} // namespace thesis
