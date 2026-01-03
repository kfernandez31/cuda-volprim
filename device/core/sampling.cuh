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
#include "thesis/device/utils/set.h"
#include "thesis/device/utils/vector.h"

#include <curand_kernel.h>
#include <optix.h>

#include <math.h>

namespace thesis {
namespace device {

namespace math = ::thesis::common::math;

using PrimsSet = utils::Set<uint, consts::ACTIVE_PRIMS_CAPACITY>;
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

//  inverse CDF for τ
__forceinline__ __device__ float sample_target_optical_depth(float uniform_sample) {
    // Inverse transform sampling from exponential distribution:
    // PDF:   p(τ) = e^(-τ)
    // CDF:   F(τ) = 1 - e^(-τ)
    // Inverse CDF: τ = -ln(1 - ξ), ξ ∈ [0,1)

    // Clamp to avoid log(0), which would be infinite
    return -math::log(math::max(1.0f - uniform_sample, consts::MIN_RANDOM_SAMPLE));
}

__forceinline__ __device__ float optical_depth_accumulated(const geometry::Ray& ray,
                                                           const PrimsSet& prims, float t0,
                                                           float t1) {
    auto tau = 0.0f;

#pragma unroll 4
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        const auto prim_tau = prim.optical_depth(ray, t0, t1);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        // Check for sentinel value and propagate it
        if (prim_tau < 0.0f) {
            return -1.0f;  // Propagate error sentinel
        }
#endif  // THESIS_ENABLE_NUMERICAL_GUARDS

        tau += prim_tau;
    }

    return tau;
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

__forceinline__ __device__ float3 integrate_primitives(const geometry::Ray& ray,
                                                       const PrimsSet& prims, float t0, float t1) {
    float3 result = make_float3(0.0f);

#pragma unroll 4
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        result += prim.density_integral(ray, t0, t1);
    }

    return result;
}

// bisection solver for τ(t) = χ
__device__ float sample_distance_bisection(const geometry::Ray& ray, const PrimsSet& prims,
                                           float tau_needed, float t0, float t1) {
    static constexpr size_t MAX_ITER = 4;

    if (tau_needed <= consts::BISECTION_TAU_EPS) {
        // Scattering too close to current ray origin, skip bisection
        return t0;
    }

    auto t_lo = t0;
    auto t_hi = t1;

    for (size_t i = 0; i < MAX_ITER && (t_hi - t_lo) > consts::BISECTION_DISTANCE_EPS; ++i) {
        const auto t_mid = math::midpoint(t_lo, t_hi);
        const auto tau = optical_depth_accumulated(ray, prims, t_lo, t_mid);

        if (tau >= tau_needed)
            t_hi = t_mid;
        else
            t_lo = t_mid;
    }

    return (t_hi - t_lo <= consts::BISECTION_DISTANCE_EPS) ? t_hi : math::midpoint(t_lo, t_hi);
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

// Helper: Collect and sort all ray-primitive hits
// Returns sorted hit buffer. Also returns Miss payload if provided (for scattering event path).
__device__ HitBuffer collect_and_sort_hits(const geometry::Ray& ray, const PrimsSet& active_prims,
                                           payloads::Miss* out_miss = nullptr) {
    HitBuffer hit_buffer;
    init_hit_buffer_sentinels(hit_buffer);

    // STEP 1: For primitives we're inside, compute exits analytically
    const auto active_size = active_prims.size();
    for (size_t idx = 0; idx < active_size; ++idx) {
        const auto prim_idx = active_prims[idx];
        const auto& prim = launch_params.primitives_[prim_idx];
        auto isect = common::geometry::intersect_ellipsoid(ray, prim);

        if (isect.starts_inside()) {
            (void) hit_buffer.emplace_back(isect.t_exit, prim_idx, true);
        }
    }

    // STEP 2: Trace with backface culling to get NEW entries
    const size_t num_computed_exits = hit_buffer.size();
    auto miss = trace_ch_collect(ray, 0.0f, consts::INF_F, hit_buffer);
    if (out_miss)
        *out_miss = miss;  // Optionally return Miss payload

    // STEP 3: For each traced entry, compute exit analytically
    const size_t total_after_trace = hit_buffer.size();
    for (size_t i = num_computed_exits; i < total_after_trace; ++i) {
        const auto& entry = hit_buffer[i];
        const auto& prim = launch_params.primitives_[entry.prim_idx];

        // Compute w_len2 once (optimization: avoid recomputing in compute_exit_from_entry)
        const auto w = prim.transform_dir_local(ray.direction_);
        const auto w_len2 = math::length2(w);

        const float exit_t =
            common::geometry::compute_exit_from_entry(ray, entry.t_hit, prim, w_len2);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
        if (exit_t > entry.t_hit) {
            if (!hit_buffer.emplace_back(exit_t, entry.prim_idx, true)) {
                hit_buffer[i].t_hit = consts::INF_F;
            }
        } else {
            hit_buffer[i].t_hit = consts::INF_F;
        }
#else
        (void) hit_buffer.emplace_back(exit_t, entry.prim_idx, true);
#endif  // THESIS_ENABLE_NUMERICAL_GUARDS
    }

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
    // Remove invalidated entries
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < hit_buffer.size(); ++read_idx) {
        if (hit_buffer[read_idx].t_hit < consts::INF_F) {
            if (write_idx != read_idx) {
                hit_buffer[write_idx] = hit_buffer[read_idx];
            }
            ++write_idx;
        }
    }
    hit_buffer.resize(write_idx);
#endif  // THESIS_ENABLE_NUMERICAL_GUARDS

    // STEP 4: Sort hits by t-value
    sort(hit_buffer);

    return hit_buffer;
}

// Process result enum for cluster processing callbacks
enum class ClusterProcessResult {
    CONTINUE,   // Continue to next cluster
    EARLY_EXIT  // Stop processing immediately
};

// Generic cluster processor: processes sorted hit buffer in t-value clusters
// Calls user callbacks for segment processing and ray-started-inside handling
// This eliminates code duplication between sample_scattering_event and
// compute_optical_depth_along_ray
template <typename SegmentCallback, typename RayStartedInsideCallback>
__device__ void process_hit_clusters(const geometry::Ray& ray, const HitBuffer& hit_buffer,
                                     PrimsSet& active_prims, SegmentCallback&& on_segment,
                                     RayStartedInsideCallback&& on_ray_started_inside) {
    float t_prev_hit = 0.0f;

    for (size_t i = 0; i < hit_buffer.size();) {
        const float t_current = hit_buffer[i].t_hit;

        // User-defined segment processing (returns CONTINUE or EARLY_EXIT)
        const auto result = on_segment(t_prev_hit, t_current);
        if (result == ClusterProcessResult::EARLY_EXIT) {
            return;
        }

        // Process ALL hits at this t-value (cluster processing for coincident surfaces)
        size_t j = i;
        while (j < hit_buffer.size() &&
               math::abs(hit_buffer[j].t_hit - t_current) < consts::HIT_COINCIDENCE_EPS) {
            const auto& hit = hit_buffer[j];

            // Update active primitives set
            if (!hit.is_exit) {
                (void) active_prims.insert(hit.prim_idx);
            } else {
                if (active_prims.contains(hit.prim_idx)) {
                    // Normal paired exit - remove from active set
                    (void) active_prims.erase(hit.prim_idx);
                } else {
                    // Ray started inside this primitive (no entry was detected)
                    // Manually integrate from ray origin to this exit
                    const auto result = on_ray_started_inside(hit.prim_idx, t_current);
                    if (result == ClusterProcessResult::EARLY_EXIT) {
                        return;
                    }
                }
            }

            ++j;
        }

        t_prev_hit = t_current;
        i = j;  // Move to next cluster
    }
}

__device__ bool sample_scattering_event(
    const geometry::Ray& ray, curandState& rng,
    optix::ScatteringEvent<consts::ACTIVE_PRIMS_CAPACITY>& event, payloads::Miss& miss) {
    const auto chi = random::sample_uniform(rng);
    const auto tau_target = sample_target_optical_depth(chi);

    auto& active_prims = event.active_prims_;

    // Collect and sort all hits (also captures Miss payload)
    auto hit_buffer = collect_and_sort_hits(ray, active_prims, &miss);

    // If no hits, ray escaped
    if (hit_buffer.empty()) {
        active_prims.clear();
        return false;
    }

    // Process hits incrementally until scattering occurs
    float tau_cumulative = 0.0f;
    bool scattered = false;

    process_hit_clusters(
        ray, hit_buffer, active_prims,
        // Segment callback: integrate optical depth and check for scattering
        [&](float t_prev, float t_current) -> ClusterProcessResult {
            const auto tau_segment =
                optical_depth_accumulated(ray, active_prims, t_prev, t_current);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
            // Check for sentinel error value (negative optical depth is invalid)
            if (tau_segment < 0.0f) {
                // Force ray to escape to avoid infinite loop
                return ClusterProcessResult::EARLY_EXIT;
            }
#endif  // THESIS_ENABLE_NUMERICAL_GUARDS

            // Check if scattering occurs before reaching this cluster
            if (tau_cumulative + tau_segment >= tau_target) {
                const auto tau_needed = tau_target - tau_cumulative;
                const auto t_scatter =
                    sample_distance_bisection(ray, active_prims, tau_needed, t_prev, t_current);

                event.t_hit_ = t_scatter;
                event.position_ = ray.at(t_scatter);
                event.direction_ = sample_phase(rng);

                scattered = true;
                return ClusterProcessResult::EARLY_EXIT;
            }

            tau_cumulative += tau_segment;
            return ClusterProcessResult::CONTINUE;
        },
        // Ray-started-inside callback: handle primitives we're already inside
        [&](uint prim_idx, float t_exit) -> ClusterProcessResult {
            const auto& prim = launch_params.primitives_[prim_idx];
            const auto tau_from_origin = prim.optical_depth(ray, 0.0f, t_exit);

#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
            // Check for sentinel error value (negative optical depth is invalid)
            if (tau_from_origin < 0.0f) {
                // Force ray to escape to avoid infinite loop
                return ClusterProcessResult::EARLY_EXIT;
            }
#endif  // THESIS_ENABLE_NUMERICAL_GUARDS

            tau_cumulative += tau_from_origin;
            return ClusterProcessResult::CONTINUE;
        });

    // miss already set from trace_ch_collect
    return scattered;
}

__device__ float3 compute_optical_depth_along_ray(const geometry::Ray& ray,
                                                  PrimsSet& active_prims) {
    auto acc_optical_depth = make_float3(0.0f);

    // Collect and sort all hits (Miss payload not needed, so don't capture it)
    auto hit_buffer = collect_and_sort_hits(ray, active_prims);

    // If no hits, no geometry to process
    if (hit_buffer.empty()) {
        return make_float3(0.0f);
    }

    // Store final t_prev for the tail segment integration
    float final_t_prev = 0.0f;

    process_hit_clusters(
        ray, hit_buffer, active_prims,
        // Segment callback: integrate density along segment
        [&](float t_prev, float t_current) -> ClusterProcessResult {
            acc_optical_depth += integrate_primitives(ray, active_prims, t_prev, t_current);
            final_t_prev = t_current;  // Track last t for tail integration
            return ClusterProcessResult::CONTINUE;
        },
        // Ray-started-inside callback: handle primitives we're already inside
        [&](uint prim_idx, float t_exit) -> ClusterProcessResult {
            const auto& prim = launch_params.primitives_[prim_idx];
            acc_optical_depth += prim.density_integral(ray, 0.0f, t_exit);
            return ClusterProcessResult::CONTINUE;
        });

    // Integrate remaining active primitives to infinity
    acc_optical_depth += integrate_primitives(ray, active_prims, final_t_prev);

    return acc_optical_depth;
}

}  // namespace device
}  // namespace thesis
