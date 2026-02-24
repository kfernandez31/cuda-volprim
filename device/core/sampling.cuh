#pragma once

#include "core/constants.cuh"
#include "core/hit_record.cuh"
#include "core/launch_params.cuh"
#include "core/random.cuh"
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

    // Sample chi for the inverse CDF computation
    const auto chi = random::sample_uniform(rng);

    auto& active_prims = event.active_prims_;

    // Pre-populate active_prims with primitives containing ray origin
    const size_t num_primitives = launch_params.primitives_.size();
#pragma unroll 4
    for (size_t i = 0; i < num_primitives; ++i) {
        const auto& prim = launch_params.primitives_[i];
        if (common::geometry::point_inside_ellipsoid(ray.origin_, prim)) {
            (void) active_prims.insert(static_cast<uint>(i));
        }
    }

    // Collect all entry hits (no sorting, exits computed lazily)
    auto hit_buffer = collect_hits(ray, &miss);

    // If no hits AND we're not inside any primitives, ray escaped into empty space
    if (hit_buffer.empty() && active_prims.empty()) {
        return false;
    }

    // ARGMIN APPROACH: Find the minimum scatter distance across all active Gaussians
    float t_scatter_min = consts::INF_F;
    uint scatter_prim_idx = UINT_MAX;

    // Check primitives we started inside (use full intersection for exit)
#pragma unroll 4
    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];
        const float t_scatter = prim.inv_cdf(ray, chi);

        if (t_scatter >= 0.0f && t_scatter < t_scatter_min) {
            auto isect = common::geometry::intersect_ellipsoid(ray, prim);
            if (t_scatter <= isect.t_exit) {
                t_scatter_min = t_scatter;
                scatter_prim_idx = prim_idx;
            }
        }
    }

    // Check primitives we enter along the ray (use optimized exit computation)
#pragma unroll 4
    for (const auto& hit : hit_buffer) {
        const auto& prim = launch_params.primitives_[hit.prim_idx];
        const float t_scatter = prim.inv_cdf(ray, chi);

        if (t_scatter >= hit.t_hit && t_scatter < t_scatter_min) {
            const auto w = prim.transform_dir_local(ray.direction_);
            const auto w_len2 = math::length2(w);
            const float t_exit = common::geometry::compute_exit_from_entry(
                ray, hit.t_hit, prim, w_len2);

            if (t_scatter <= t_exit) {
                t_scatter_min = t_scatter;
                scatter_prim_idx = hit.prim_idx;
            }
        }
    }

    if (t_scatter_min >= consts::INF_F) {
        // No scattering occurred - ray escaped
        // Build the complete set of primitives the ray passed through for optical depth calculation
        PrimsSet all_traversed_prims;

        // Add primitives we started inside
#pragma unroll 4
        for (auto prim_idx : active_prims) {
            (void) all_traversed_prims.insert(prim_idx);
        }

        // Add all primitives we entered (from hit buffer)
#pragma unroll 4
        for (const auto& hit : hit_buffer) {
            (void) all_traversed_prims.insert(hit.prim_idx);
        }

        active_prims = all_traversed_prims;
        return false;
    }

    // Rebuild active_prims at the scatter point
    PrimsSet final_active_prims;

    // Check primitives we started inside (use full intersection for exit)
#pragma unroll 4
    for (auto prim_idx : active_prims) {
        const auto& prim = launch_params.primitives_[prim_idx];
        auto isect = common::geometry::intersect_ellipsoid(ray, prim);
        if (t_scatter_min <= isect.t_exit) {
            (void) final_active_prims.insert(prim_idx);
        }
    }

    // Check primitives we entered before scatter (use optimized exit computation)
    // Note: hit_buffer is unsorted (OptiX anyhit calls are in unspecified order)
    // Must check all hits, cannot use early break optimization
#pragma unroll 4
    for (const auto& hit : hit_buffer) {
        if (hit.t_hit > t_scatter_min) continue;  // Skip hits after scatter point

        const auto& prim = launch_params.primitives_[hit.prim_idx];
        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);
        const float t_exit = common::geometry::compute_exit_from_entry(
            ray, hit.t_hit, prim, w_len2);

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
