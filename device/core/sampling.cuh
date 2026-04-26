#pragma once

#include "core/constants.cuh"
#include "core/hit_record.cuh"
#include "core/launch_params.cuh"
#include "core/random.cuh"
#include "core/sorting.cuh"
#include "core/trace.cuh"

#include "thesis/common/geometry/intersection.h"
#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/optix/scattering_event.h"
#include "thesis/device/payloads/miss.h"
#include "thesis/device/utils/bit_vector.h"
#include "thesis/device/utils/compact_set.h"
#include "thesis/device/utils/vector.h"

#include <optix.h>

#include <math.h>
#include <type_traits>

namespace thesis {
namespace device {

namespace math = ::thesis::common::math;

// PrimsSet: tracks which primitives are active (overlapping) at the current ray point.
// For small scenes (≤256 prims): BitVector — O(1) ops, indexed by primitive ID.
// For large scenes: CompactSet — O(k) ops, decoupled from scene size.
using PrimsSet = std::conditional_t<(consts::MAX_PRIMITIVES <= 256),
                                    utils::BitVector<((consts::MAX_PRIMITIVES + 63) & ~size_t{63})>,
                                    utils::CompactSet<prim_idx_t, consts::MAX_ACTIVE_PRIMS> >;
using HitBuffer = utils::StaticVector<HitRecord, consts::HIT_BUFFER_CAPACITY>;

__forceinline__ __device__ float3 sample_phase(random::PCG32& rng) {
    // Isotropic phase function: uniform over sphere
    // Role:
    // Determines in which direction light scatters after the event.

    // Mechanism:
    // Draws a new direction from a phase function, which is a PDF over the unit sphere. Controls
    // anisotropy of scattering.
    auto sample = random::sample_uniform_2d(rng);
    auto z = math::fma(-2.0f, sample.x, 1.0f);                     // 1.0 - 2.0*x
    auto r = math::sqrt(math::max(0.0f, math::fma(-z, z, 1.0f)));  // 1.0 - z²
    auto phi = math::TWO_PI_F * sample.y;
    return make_float3(r * math::cos(phi), r * math::sin(phi), z);
}

__forceinline__ __device__ float3 integrate_primitives(const geometry::Ray& ray,
                                                       const PrimsSet& prims, float t0) {
    float3 result = make_float3(0.0f);

    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        result += prim.density_integral(ray, t0);
    }

    return result;
}

__device__ __forceinline__ float3 evaluate_albedo(float3 pos, const PrimsSet& prims) {
    auto accum_albedo = make_float3(0.0f);
    auto accum_weight = 0.0f;

    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];

        const auto sigma_t = prim.optical_thickness_;  // extinction coefficient
        const auto albedo = prim.albedo_;
        const auto pdf = prim.pdf(pos);  // density at pos

        const auto weight = sigma_t * pdf;
        accum_albedo += albedo * weight;
        accum_weight += weight;
    }

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
    if (accum_weight <= 0.0f) {
        printf("ERROR: evaluate_albedo called with zero total weight (empty active_prims?)\n");
        return make_float3(0.0f);
    }
#endif  // THESIS_ENABLE_NUMERICAL_GUARDS
    return accum_albedo * math::rcp(accum_weight);
}

using EventBuffer = utils::StaticVector<HitRecord, 2 * consts::HIT_BUFFER_CAPACITY>;

// Helper: Collect ray-primitive entry hits (exits computed lazily)
// Clears and fills the provided hit buffer. Also returns Miss payload if provided.
__device__ void collect_hits(const geometry::Ray& ray, HitBuffer& hit_buffer,
                             payloads::Miss* out_miss = nullptr) {
    hit_buffer.clear();

    // STEP 1: Trace with backface culling to get entry hits (no exits!)
    auto miss = trace_ch_collect(ray, 0.0f, consts::INF_F, hit_buffer);
    if (out_miss)
        *out_miss = miss;  // Optionally return Miss payload

        // No exit computation - exits will be computed on-demand in argmin loop
        // No sorting - argmin doesn't need sorted hits

#ifdef DEBUG
    if (hit_buffer.full()) {
        printf("WARNING: Hit buffer overflow (%zu/%zu entries) — ray may be biased\n",
               hit_buffer.size(), hit_buffer.capacity());
    }
#endif
}

// Compute optical depth along escape ray using segment-by-segment integration
// Separated into __noinline__ to isolate EventBuffer + sort register pressure from scatter path
__device__ __noinline__ float3 compute_escape_optical_depth(const geometry::Ray& ray,
                                                            const PrimsSet& active_prims,
                                                            const HitBuffer& hit_buffer) {
    EventBuffer events;

    // Build exit events for primitives containing the ray origin
    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];
        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit = common::geometry::compute_exit_from_entry(ray, 0.0f, prim, w_len2);
        if (t_exit > 0.0f && t_exit < consts::INF_F) {
            events.emplace_back(t_exit, prim_idx, true);
        }
    }

    // Build entry+exit events for ray-intersected primitives
    for (const auto& hit : hit_buffer) {
        const auto& prim = launch_params.primitives_[hit.prim_idx];

        events.emplace_back(hit.t_hit, hit.prim_idx, false);

        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit =
            common::geometry::compute_exit_from_entry(ray, hit.t_hit, prim, w_len2);

        if (t_exit > hit.t_hit && t_exit < consts::INF_F) {
            events.emplace_back(t_exit, hit.prim_idx, true);
        }
    }

    sort(events);

    float t_prev = 0.0f;
    PrimsSet current_active = active_prims;
    float3 acc_optical_depth = make_float3(0.0f);

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];

        if (ev.t_hit > t_prev) {
            for (auto prim_idx : current_active) {
                const auto& prim = launch_params.primitives_[prim_idx];
                const auto tau = prim.optical_depth(ray, t_prev, ev.t_hit);
                acc_optical_depth += make_float3(tau);
            }
        }

        if (ev.is_exit) {
            current_active.erase(ev.prim_idx);
        } else {
            (void) current_active.insert(ev.prim_idx);
        }

        t_prev = ev.t_hit;
    }

    return acc_optical_depth;
}

// Sample scattering event using argmin approach (no sorting!)
// Based on Analog Decomposition Tracking theorem from SDTracking paper (Section 4.1):
// The minimum of independent inverse CDFs gives the same distribution as sorting
__device__ __noinline__ bool sample_scattering_event(const geometry::Ray& ray, random::PCG32& rng,
                                                     optix::ScatteringEvent<PrimsSet>& event,
                                                     payloads::Miss& miss, HitBuffer& hit_buffer) {
    auto& active_prims = event.active_prims_;

    const size_t num_primitives = launch_params.primitives_.size();

    for (size_t i = 0; i < num_primitives; ++i) {
        const auto& prim = launch_params.primitives_[i];
        if (common::geometry::point_inside_ellipsoid(ray.origin_, prim)) {
            (void) active_prims.insert(static_cast<prim_idx_t>(i));
        }
    }

    collect_hits(ray, hit_buffer, &miss);

    if (hit_buffer.empty() && active_prims.empty()) {
        // No primitives in scene - set optical depth to zero (ray escapes freely)
        event.escape_optical_depth_ = make_float3(0.0f);
        return false;
    }

    float t_scatter_min = consts::INF_F;

    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];

        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit = common::geometry::compute_exit_from_entry(ray, 0.0f, prim, w_len2);

        // Sample INDEPENDENT free-flight distance per primitive (ADT requirement)
        // Transform uniform sample to optical depth threshold: τ = -log(1-χ)
        const float chi_i = random::sample_uniform(rng);
        const float tau_i = -math::log(math::max(1.0f - chi_i, 1e-10f));
        const float t_scatter = prim.inv_cdf(ray, tau_i);

        if (t_scatter >= 0.0f && t_scatter < t_scatter_min && t_scatter <= t_exit) {
            t_scatter_min = t_scatter;
        }
    }

    for (const auto& hit : hit_buffer) {
        const auto& prim = launch_params.primitives_[hit.prim_idx];

        // Sample INDEPENDENT free-flight distance per primitive (ADT requirement)
        // Transform uniform sample to optical depth threshold: τ = -log(1-χ)
        const float chi_j = random::sample_uniform(rng);
        const float tau_j = -math::log(math::max(1.0f - chi_j, 1e-10f));

        // Sample scatter from full Gaussian CDF, then check if result falls within
        // the BVH sphere [t_hit, t_exit]. For dense media with albedo≈0, scatter
        // naturally fails (scatter point precedes entry) → escape path computes
        // correct Beer-Lambert transmittance.
        const float t_scatter = prim.inv_cdf(ray, tau_j);

        if (t_scatter >= hit.t_hit && t_scatter < t_scatter_min) {
            const auto w = prim.transform_dir_local(ray.direction_);
            const auto w_len2 = math::length2(w);
            const float t_exit =
                common::geometry::compute_exit_from_entry(ray, hit.t_hit, prim, w_len2);

            if (t_scatter <= t_exit) {
                t_scatter_min = t_scatter;
            }
        }
    }

    if (t_scatter_min >= consts::INF_F) {
        event.escape_optical_depth_ = compute_escape_optical_depth(ray, active_prims, hit_buffer);
        active_prims.clear();
        return false;
    }

    // Rebuild active_prims at the scatter point
    PrimsSet final_active_prims;

    // Recompute exits for primitives containing the ray origin
    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];
        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit = common::geometry::compute_exit_from_entry(ray, 0.0f, prim, w_len2);
        if (t_scatter_min <= t_exit) {
            (void) final_active_prims.insert(prim_idx);
        }
    }

    for (const auto& hit : hit_buffer) {
        if (hit.t_hit > t_scatter_min)
            continue;  // Skip hits after scatter point

        const auto& prim = launch_params.primitives_[hit.prim_idx];
        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit =
            common::geometry::compute_exit_from_entry(ray, hit.t_hit, prim, w_len2);

        if (t_scatter_min <= t_exit) {
            (void) final_active_prims.insert(hit.prim_idx);
        }
    }

    active_prims = final_active_prims;

    // Set the scattering event
    event.t_hit_ = t_scatter_min;
    event.position_ = ray.at(t_scatter_min);
    event.direction_ = sample_phase(rng);

    return true;
}

__device__ float3 compute_optical_depth_along_ray(const geometry::Ray& ray,
                                                  const PrimsSet& active_prims) {
    // Simple approach: integrate all active primitives from ray origin to infinity
    // active_prims already contains the correct set from sample_scattering_event
    auto acc_optical_depth = make_float3(0.0f);

    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];
        acc_optical_depth += prim.density_integral(ray, 0.0f);
    }

    return acc_optical_depth;
}

}  // namespace device
}  // namespace thesis
