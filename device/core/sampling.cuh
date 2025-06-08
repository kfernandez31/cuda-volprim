#pragma once

#include "core/launch_params.cuh"
#include "core/random.cuh"
#include "core/trace.cuh"

#include "thesis/device/utils/vector.h"
#include "thesis/device/utils/set.h"
#include "thesis/common/utils/math.h"
#include "thesis/device/optix/scattering_event.h"
#include "thesis/device/utils/optional.h"
#include "thesis/device/utils/result.h"
#include "thesis/device/payloads/miss.h"

#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/types.h"

#include <optix.h>
#include <math.h>
#include <sutil/vec_math.h>
#include <curand_kernel.h>

namespace thesis {
namespace device {

namespace consts {
    constexpr auto MAX_PRIMS = 2137u;
} // namespace consts

using PrimsSet = utils::Set<uint, consts::MAX_PRIMS>;

__forceinline__ __device__ float3 sample_phase(curandState& rng) {
    namespace math = thesis::common::math;
    // Isotropic phase function: uniform over sphere
    // Role:
    // Determines in which direction light scatters after the event.

    // Mechanism:
    // Draws a new direction from a phase function, which is a PDF over the unit sphere. Controls anisotropy of scattering.
    auto sample = random::sample_uniform_2d(rng);
    auto z = 1.0f - 2.0f * sample.x;
    auto r = sqrtf(fmaxf(0.0f, 1.0f - math::pow2(z)));
    auto phi = math::TWO_PI_F * sample.y;
    return make_float3(r * cosf(phi), r * sinf(phi), z); // direction, already unit
}

//  inverse CDF for τ
__forceinline__ __device__ float sample_target_optical_depth(float uniform_sample) {
    // Inverse transform sampling from exponential distribution:
    // PDF:   p(τ) = e^(-τ)
    // CDF:   F(τ) = 1 - e^(-τ)
    // Inverse CDF: τ = -ln(1 - ξ), ξ ∈ [0,1)

    // Clamp to avoid log(0), which would be infinite
    return -logf(fmaxf(1.0f - uniform_sample, 1e-6f));
}

__forceinline__ __device__ float optical_depth_accumulated(
    const geometry::Ray& ray,
    float2 segment,
    const PrimsSet& prims
) {
    auto tau = 0.0f;

    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        tau += prim.optical_depth(ray, segment);
    }

    return tau;
}

template <typename T>
__forceinline__ __device__ float3 integrate_primitives(
    const geometry::Ray& ray,
    T t,
    const PrimsSet& prims
) {
    float3 result = make_float3(0.0f);

    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        result += prim.density_integral(ray, t);
    }

    return result;
}

// TODO(kacper) potential to optimize: don't invert, select random position on the segment which we call the ... and here the conversation with Jorge broke so I don't know what he meant
// bisection solver for τ(t) = χ
__device__ float sample_distance_bisection(
    const geometry::Ray& ray,
    float2 segment,
    float tau_needed,
    const PrimsSet& prims
) {
    constexpr auto MAX_ITER = 24u;
    constexpr auto EPS = 1e-4f;

    auto t_lo = segment.x;
    auto t_hi = segment.y;

    for (size_t i = 0; i < MAX_ITER && (t_hi - t_lo) > EPS; ++i) {
        auto t_mid = 0.5f * (t_lo + t_hi);
        auto tau = optical_depth_accumulated(ray, make_float2(t_lo, t_mid), prims);

        if (tau >= tau_needed)
            t_hi = t_mid;
        else
            t_lo = t_mid;
    }

    return (t_hi - t_lo <= EPS) ? t_hi : 0.5f * (t_lo + t_hi);
}

__device__ __forceinline__ float3 evaluate_albedo(
    float3 pos, 
    const PrimsSet& prims
) {
    auto accum_albedo = make_float3(0.0f);
    auto accum_weight = 0.0f;

    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];

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

__device__ bool sample_scattering_event(const geometry::Ray& ray, curandState& rng, optix::ScatteringEvent<consts::MAX_PRIMS>& event, payloads::Miss& miss) {
    auto t_total = 0.0f;
    auto tau_cumulative = 0.0f;
    
    const auto chi = random::sample_uniform(rng);
    const auto tau_target = sample_target_optical_depth(chi);
    
    auto& active_prims = event.active_prims_;
    while (!active_prims.full()) {
        const auto result = trace_ch(ray, t_total);
    
        if (!result) {
            miss = result.unwrap_err();
            return false;
        }

        const auto& hit = result.unwrap();
        const auto t_hit = hit.t_hit;
        const auto prim_idx = hit.prim_idx;
        const auto is_exit = hit.is_exit;

        const auto segment = make_float2(t_total, t_hit);
        const auto tau_segment = optical_depth_accumulated(ray, segment, active_prims);

        // scattering occurred
        // [                                t              ]
        // ^- tau_cumulative & t_total      ^- tau_target & t      ^-tau_target + tau_segment & t_hit
        if (tau_cumulative + tau_segment >= tau_target) {
            auto tau_needed = tau_target - tau_cumulative;
            auto t = sample_distance_bisection(ray, segment, tau_needed, active_prims);

            event.t_hit_ = t;
            event.position_ = ray.at(t);
            event.direction_ = sample_phase(rng);
            return true;
        }

        if (is_exit) {
            active_prims.erase(prim_idx);
        } else {
            active_prims.insert(prim_idx);
        }

        t_total = t_hit;
        tau_cumulative += tau_segment;
    }

    auto color = launch_params.env_map_.sample(ray.direction_);
    miss = payloads::Miss(color);
    return false;
}

__device__ float3 compute_optical_depth_along_ray(const geometry::Ray& ray) {
    auto acc_optical_depth = make_float3(0.0f);
    auto t_old = 0.0f;

    PrimsSet active_prims;
    while (!active_prims.full()) {
        const auto result = trace_ch(ray, t_old);

        if (!result) {
            break;
        }

        const auto& payload = result.unwrap();
        const auto t_new = payload.t_hit;
        const auto prim_idx = payload.prim_idx;
        const auto is_exit = payload.is_exit;

        acc_optical_depth += integrate_primitives(ray, make_float2(t_old, t_new), active_prims);

        if (is_exit) {
            active_prims.erase(prim_idx);
        } else {
            active_prims.insert(prim_idx);
        }

        t_old = t_new;
    }

    // drain remaining primitives until infinity
    acc_optical_depth += integrate_primitives(ray, t_old, active_prims);
    return acc_optical_depth;
}

} // namespace device
} // namespace thesis
