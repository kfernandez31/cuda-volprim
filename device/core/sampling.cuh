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
#include "thesis/device/utils/vector.h"

#include <curand_kernel.h>
#include <optix.h>

#include <math.h>

namespace thesis {
namespace device {

namespace math = ::thesis::common::math;

using PrimsSet = utils::BitVector<consts::ACTIVE_PRIMS_CAPACITY>;
using HitBuffer = utils::StaticVector<HitRecord, consts::HIT_BUFFER_CAPACITY>;

__forceinline__ __device__ float3 sample_phase(curandState& rng) {
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

#pragma unroll 4
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        result += prim.density_integral(ray, t0);
    }

    return result;
}

__device__ __forceinline__ float3 evaluate_albedo(float3 pos, const PrimsSet& prims) {
    auto accum_albedo = make_float3(0.0f);
    auto accum_weight = 0.0f;

#pragma unroll 4
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];

        const auto sigma_t = prim.optical_thickness_;  // extinction coefficient
        const auto albedo = prim.albedo_;
        const auto pdf = prim.pdf(pos);  // density at pos

        const auto weight = sigma_t * pdf;
        accum_albedo += albedo * weight;
        accum_weight += weight;
    }

    // Invariant: This function is only called after scattering occurs, which requires non-empty
    // active_prims
    return accum_albedo * math::rcp(accum_weight);
}

// Helper: Collect ray-primitive entry hits (exits computed lazily)
// Returns unsorted hit buffer containing only entry hits. Also returns Miss payload if provided.
__device__ HitBuffer collect_hits(const geometry::Ray& ray, payloads::Miss* out_miss = nullptr) {
    HitBuffer hit_buffer;

    // STEP 1: Trace with backface culling to get entry hits (no exits!)
    auto miss = trace_ch_collect(ray, 0.0f, consts::INF_F, hit_buffer);
    if (out_miss)
        *out_miss = miss;  // Optionally return Miss payload

    // No exit computation - exits will be computed on-demand in argmin loop
    // No sorting - argmin doesn't need sorted hits

    return hit_buffer;
}

// Sample scattering event using argmin approach (no sorting!)
// Based on Analog Decomposition Tracking theorem from SDTracking paper (Section 4.1):
// The minimum of independent inverse CDFs gives the same distribution as sorting
__device__ bool sample_scattering_event(
    const geometry::Ray& ray, curandState& rng,
    optix::ScatteringEvent<consts::ACTIVE_PRIMS_CAPACITY>& event, payloads::Miss& miss) {

    auto& active_prims = event.active_prims_;

    const size_t num_primitives = launch_params.primitives_.size();
#pragma unroll 4
    for (size_t i = 0; i < num_primitives; ++i) {
        const auto& prim = launch_params.primitives_[i];
        if (common::geometry::point_inside_ellipsoid(ray.origin_, prim)) {
            (void) active_prims.insert(static_cast<prim_idx_t>(i));
        }
    }

    auto hit_buffer = collect_hits(ray, &miss);

    if (hit_buffer.empty() && active_prims.empty()) {
        // No primitives in scene - set optical depth to zero (ray escapes freely)
        event.escape_optical_depth_ = make_float3(0.0f);
        return false;
    }

    float t_scatter_min = consts::INF_F;

    // Cache exits for primitives we start inside (eliminates redundant computation)
    // These exits are reused in both escape case (event collection) and scatter case (active_prims rebuild)
    struct CachedExit { // TODO: we could use hit_record.cuh instead for reducing redundancy
        prim_idx_t prim_idx;
        float t_exit;
    };
    utils::StaticVector<CachedExit, consts::ACTIVE_PRIMS_CAPACITY> cached_exits;

#pragma unroll 4
    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];

        // ALWAYS compute and cache exit (needed for escape case optical depth)
        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit = common::geometry::compute_exit_from_entry(ray, 0.0f, prim, w_len2);
        cached_exits.emplace_back(CachedExit{prim_idx, t_exit});

        // Sample INDEPENDENT free-flight distance per primitive (ADT requirement)
        // Transform uniform sample to optical depth threshold: τ = -log(1-χ)
        const float chi_i = random::sample_uniform(rng);
        const float tau_i = -math::log(math::max(1.0f - chi_i, 1e-10f));
        const float t_scatter = prim.inv_cdf(ray, tau_i);

        if (t_scatter >= 0.0f && t_scatter < t_scatter_min && t_scatter <= t_exit) {
            t_scatter_min = t_scatter;
        }
    }

#pragma unroll 4
    for (const auto& hit : hit_buffer) {
        const auto& prim = launch_params.primitives_[hit.prim_idx];

        // Sample INDEPENDENT free-flight distance per primitive (ADT requirement)
        // Transform uniform sample to optical depth threshold: τ = -log(1-χ)
        const float chi_j = random::sample_uniform(rng);
        const float tau_j = -math::log(math::max(1.0f - chi_j, 1e-10f));

        // For entry hits, sample from the entry point onward
        // Note: We don't shift the ray origin because inv_cdf integrates from ray.origin
        // Instead, we sample and then check the result is after the entry point
        const float t_scatter = prim.inv_cdf(ray, tau_j);

        if (t_scatter >= hit.t_hit && t_scatter < t_scatter_min) {
            const auto w = prim.transform_dir_local(ray.direction_);
            const auto w_len2 = math::length2(w);
            const float t_exit = common::geometry::compute_exit_from_entry(ray, hit.t_hit, prim, w_len2);

            if (t_scatter <= t_exit) {
                t_scatter_min = t_scatter;
            }
        }
    }

    if (t_scatter_min >= consts::INF_F) {
        // Reuse HitRecord for event tracking (unified structure saves memory)
        // HitRecord now contains is_exit field, eliminating need for separate Event struct
        // Memory savings: Was 2N×12 bytes (Event), now reusing hit_buffer structure
        utils::StaticVector<HitRecord, 2 * consts::HIT_BUFFER_CAPACITY> events;

        // Reuse cached exits from argmin loop (eliminates redundant computation)
#pragma unroll 4
        for (const auto& cached : cached_exits) {
            if (cached.t_exit > 0.0f && cached.t_exit < consts::INF_F) {
                events.emplace_back(cached.t_exit, cached.prim_idx, true);
            }
        }

#pragma unroll 4
        for (const auto& hit : hit_buffer) {
            const auto& prim = launch_params.primitives_[hit.prim_idx];

            events.emplace_back(hit.t_hit, hit.prim_idx, false);

            const auto w = prim.transform_dir_local(ray.direction_);
            const auto w_len2 = math::length2(w);
            const float t_exit = common::geometry::compute_exit_from_entry(ray, hit.t_hit, prim, w_len2);

            if (t_exit > hit.t_hit && t_exit < consts::INF_F) {
                events.emplace_back(t_exit, hit.prim_idx, true);
            }
        }

        // Sort events by t-value using optimized adaptive sort
        sort(events);

        float t_prev = 0.0f;
        PrimsSet current_active = active_prims;  // Start with primitives we're inside // TODO: we can do this OR we would leave this set empty, and with the current implementation in which we don't hold entry events for prims we're inside, we would just ignore the fact that `erase` returns false. We understand it that conceptually the erasure is fine, but in the set the entry is simply not present. Design decision. But we can store the entry instead.
        float3 acc_optical_depth = make_float3(0.0f);

        for (size_t i = 0; i < events.size(); ++i) {
            const auto& event = events[i];

            if (event.t_hit > t_prev) {
#pragma unroll 4
                for (auto prim_idx : current_active) {
                    const auto& prim = launch_params.primitives_[prim_idx];
                    const auto tau = prim.optical_depth(ray, t_prev, event.t_hit);
                    acc_optical_depth += make_float3(tau);
                }
            }

            if (event.is_exit) {
                current_active.erase(event.prim_idx);
            } else {
                (void) current_active.insert(event.prim_idx);
            }

            t_prev = event.t_hit;
        }

        event.escape_optical_depth_ = acc_optical_depth;

        // Clear active_prims (ray escaped, no longer inside any primitives)
        active_prims.clear();

        return false;
    }

    // Rebuild active_prims at the scatter point
    PrimsSet final_active_prims;

    // Reuse cached exits from argmin loop (eliminates redundant computation)
#pragma unroll 4
    for (const auto& cached : cached_exits) {
        if (t_scatter_min <= cached.t_exit) {
            (void) final_active_prims.insert(cached.prim_idx);
        }
    }

#pragma unroll 4
    for (const auto& hit : hit_buffer) {
        if (hit.t_hit > t_scatter_min) continue;  // Skip hits after scatter point

        const auto& prim = launch_params.primitives_[hit.prim_idx];
        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit = common::geometry::compute_exit_from_entry(ray, hit.t_hit, prim, w_len2);

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

#pragma unroll 4
    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];
        acc_optical_depth += prim.density_integral(ray, 0.0f);
    }

    return acc_optical_depth;
}

}  // namespace device
}  // namespace thesis
