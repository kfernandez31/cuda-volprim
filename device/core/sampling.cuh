#pragma once

#include "core/constants.cuh"
#include "core/launch_params.cuh"
#include "core/random.cuh"
#include "core/trace.cuh"
#include "core/hit_record.cuh"
#include "core/sorting.cuh"

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

using PrimsSet = utils::Set<uint, consts::MAX_CAPACITY>;
using HitBuffer = utils::StaticVector<HitRecord, consts::MAX_CAPACITY>;

__forceinline__ __device__ float3 sample_phase(curandState& rng) {
    namespace math = thesis::common::math;
    // Isotropic phase function: uniform over sphere
    // Role:
    // Determines in which direction light scatters after the event.

    // Mechanism:
    // Draws a new direction from a phase function, which is a PDF over the unit sphere. Controls anisotropy of scattering.
    auto sample = random::sample_uniform_2d(rng);
    auto z = fmaf(-2.0f, sample.x, 1.0f);  // FMA: 1.0 + (-2.0)*x
    auto r = sqrtf(fmaxf(0.0f, fmaf(-z, z, 1.0f)));  // FMA: 1.0 + (-z)*z = 1.0 - z²
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
    const PrimsSet& prims,
    float t0, float t1
) {
    auto tau = 0.0f;

    #pragma unroll 4
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        tau += prim.optical_depth(ray, t0, t1);
    }

    return tau;
}

__forceinline__ __device__ float3 integrate_primitives(
    const geometry::Ray& ray,
    const PrimsSet& prims,
    float t0
) {
    float3 result = make_float3(0.0f);

    #pragma unroll 4
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        result += prim.density_integral(ray, t0);
    }

    return result;
}

__forceinline__ __device__ float3 integrate_primitives(
    const geometry::Ray& ray,
    const PrimsSet& prims,
    float t0, float t1
) {
    float3 result = make_float3(0.0f);

    #pragma unroll 4
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];
        result += prim.density_integral(ray, t0, t1);
    }

    return result;
}

// bisection solver for τ(t) = χ
__device__ float sample_distance_bisection(
    const geometry::Ray& ray,
    const PrimsSet& prims,
    float tau_needed,
    float t0, float t1
) {
    static constexpr size_t MAX_ITER = 4;
    static constexpr float EPS = 1e-4f;
    static constexpr float TAU_EPS = 1e-6f;

    if (tau_needed <= TAU_EPS) {
        // Scattering too close to current ray origin, skip bisection
        return t0;
    }

    auto t_lo = t0;
    auto t_hi = t1;

    for (size_t i = 0; i < MAX_ITER && (t_hi - t_lo) > EPS; ++i) {
        const auto t_mid = 0.5f * (t_lo + t_hi);
        const auto tau = optical_depth_accumulated(ray, prims, t_lo, t_mid);

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

    #pragma unroll 4
    for (auto idx : prims) {
        const auto& prim = launch_params.primitives_[idx];

        const auto sigma_t = prim.optical_thickness_;  // extinction coefficient
        const auto albedo = prim.albedo_;
        const auto pdf = prim.pdf(pos);           // density at pos

        const auto weight = sigma_t * pdf;
        accum_albedo += albedo * weight;
        accum_weight += weight;
    }

    // Invariant: This function is only called after scattering occurs, which requires non-empty active_prims
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
    return accum_albedo * math::safe_rcp(accum_weight);
#else
    return accum_albedo / accum_weight;
#endif
}

__device__ bool sample_scattering_event(const geometry::Ray& ray, curandState& rng, optix::ScatteringEvent<consts::MAX_CAPACITY>& event, payloads::Miss& miss) {
    const auto chi = random::sample_uniform(rng);
    const auto tau_target = sample_target_optical_depth(chi);

    auto& active_prims = event.active_prims_;

    if (is_debug_thread()) {
        printf("\n=== sample_scattering_event ENTRY, tau_target=%.3f ===\n", tau_target);
        printf("  Ray: origin=(%.3f,%.3f,%.3f), dir=(%.3f,%.3f,%.3f)\n",
               ray.origin_.x, ray.origin_.y, ray.origin_.z,
               ray.direction_.x, ray.direction_.y, ray.direction_.z);
    }

    // Collect ALL hits along ray in one BVH traversal
    HitBuffer hit_buffer;
    trace_ch_collect(ray, 0.0f, consts::INF_F, &hit_buffer);

    if (is_debug_thread()) {
        printf("  Collected %u hits\n", static_cast<uint>(hit_buffer.size()));
    }

    // If no hits, ray escaped to environment
    if (hit_buffer.size() == 0) {
        if (is_debug_thread()) printf("  No geometry, RETURNING FALSE\n");
        active_prims.clear();  // Ray is not inside any volume
        auto color = launch_params.env_map_.sample(ray.direction_);
        miss = payloads::Miss(color);
        return false;
    }

    // Sort hits by t-value
    sort(hit_buffer);

    if (is_debug_thread()) {
        printf("  After sort, buffer size=%u:\n", static_cast<uint>(hit_buffer.size()));
        for (size_t j = 0; j < hit_buffer.size(); j++) {
            printf("    [%u] t=%.6f, prim=%u, exit=%d\n",
                   static_cast<uint>(j), hit_buffer[j].t_hit, hit_buffer[j].prim_idx, hit_buffer[j].is_exit);

            // Check for coincident hits (within 1e-6)
            if (j > 0 && fabsf(hit_buffer[j].t_hit - hit_buffer[j-1].t_hit) < 1e-6f) {
                printf("      WARNING: Coincident with previous hit (dt=%.9f)\n",
                       hit_buffer[j].t_hit - hit_buffer[j-1].t_hit);
            }
        }
    }

    // Process hits incrementally until scattering occurs
    // Key insight: Process ALL hits at the same t-value before integrating to next t
    float tau_cumulative = 0.0f;
    float t_prev_hit = 0.0f;

    for (size_t i = 0; i < hit_buffer.size(); ) {
        const float t_current = hit_buffer[i].t_hit;

        // Integrate from previous t to current t
        const auto tau_segment = optical_depth_accumulated(ray, active_prims, t_prev_hit, t_current);

        if (is_debug_thread()) {
            printf("  Processing cluster at t=%.6f, tau_segment=%.3f, tau_cumulative=%.3f, tau_target=%.3f, active_prims.size=%u\n",
                   t_current, tau_segment, tau_cumulative, tau_target, static_cast<uint>(active_prims.size()));
        }

        // Check if scattering occurs before reaching this cluster
        if (tau_cumulative + tau_segment >= tau_target) {
            auto tau_needed = tau_target - tau_cumulative;

            auto t_scatter = sample_distance_bisection(ray, active_prims, tau_needed, t_prev_hit, t_current);

            if (is_debug_thread()) {
                printf("  SCATTERING at t=%.3f, active_prims=[", t_scatter);
                bool first = true;
                for (auto prim : active_prims) {
                    if (!first) printf(",");
                    printf("%u", prim);
                    first = false;
                }
                printf("] size=%u\n", static_cast<uint>(active_prims.size()));
            }

            event.t_hit_ = t_scatter;
            event.position_ = ray.at(t_scatter);
            event.direction_ = sample_phase(rng);

            return true;
        }

        tau_cumulative += tau_segment;

        // Process ALL hits at this t-value before moving to next segment
        size_t j = i;
        while (j < hit_buffer.size() && fabsf(hit_buffer[j].t_hit - t_current) < 1e-6f) {
            const auto& hit = hit_buffer[j];
            const auto prim_idx = hit.prim_idx;
            const auto is_exit = hit.is_exit;

            if (is_debug_thread()) {
                printf("    Hit %u: t=%.6f, prim=%u, is_exit=%d\n", static_cast<uint>(j), hit.t_hit, prim_idx, is_exit);
            }

            // Update active primitives set
            if (is_exit) {
                if (!active_prims.contains(prim_idx)) {
                    // Ray started inside this primitive (no entry was detected)
                    // Manually integrate from ray origin to this exit
                    const auto& prim = launch_params.primitives_[prim_idx];
                    const auto tau_from_origin = prim.optical_depth(ray, 0.0f, t_current);
                    tau_cumulative += tau_from_origin;

                    if (is_debug_thread()) {
                        printf("      UNPAIRED EXIT: prim %u was active from ray origin, tau=[0,%.3f]=%.3f\n",
                               prim_idx, t_current, tau_from_origin);
                    }
                    // Don't try to erase - it's not in the set
                } else {
                    // Normal paired exit - remove from active set
                    if (is_debug_thread()) printf("      EXIT: erasing prim %u from active_prims\n", prim_idx);
                    if (!active_prims.erase(prim_idx)) {
                        if (is_debug_thread()) printf("      erase failed for prim %u\n", prim_idx);
                    } else if (is_debug_thread()) {
                        printf("      erased prim %u\n", prim_idx);
                    }
                }
            } else {
                if (is_debug_thread()) printf("      ENTRY: inserting prim %u into active_prims\n", prim_idx);
                if (!active_prims.insert(prim_idx)) {
                    if (is_debug_thread()) printf("      insert failed for prim %u\n", prim_idx);
                } else if (is_debug_thread()) {
                    printf("      inserted prim %u\n", prim_idx);
                }
            }

            j++;
        }

        t_prev_hit = t_current;
        i = j;  // Move to next cluster
    }

    // No scattering occurred along entire ray
    if (is_debug_thread()) {
        printf("  Processed all %u hits, no scattering, RETURNING FALSE\n", static_cast<uint>(hit_buffer.size()));
    }

    auto color = launch_params.env_map_.sample(ray.direction_);
    miss = payloads::Miss(color);
    return false;
}

__device__ float3 compute_optical_depth_along_ray(const geometry::Ray& ray, PrimsSet& active_prims) {
    auto acc_optical_depth = make_float3(0.0f);

    if (is_debug_thread()) {
        printf("\n=== compute_optical_depth_along_ray ===\n");
        printf("Ray: origin=(%.3f,%.3f,%.3f), dir=(%.3f,%.3f,%.3f)\n",
               ray.origin_.x, ray.origin_.y, ray.origin_.z,
               ray.direction_.x, ray.direction_.y, ray.direction_.z);
        printf("Initial active_prims: [");
        bool first = true;
        for (auto prim : active_prims) {
            if (!first) printf(",");
            printf("%u", prim);
            first = false;
        }
        printf("] size=%u\n", static_cast<uint>(active_prims.size()));
    }

    // Collect ALL hits along ray in one BVH traversal
    HitBuffer hit_buffer;
    trace_ch_collect(ray, 0.0f, consts::INF_F, &hit_buffer);

    if (is_debug_thread()) {
        printf("  Collected %u hits\n", static_cast<uint>(hit_buffer.size()));
    }

    // If no hits, no more geometry to process - return zero
    if (hit_buffer.size() == 0) {
        if (is_debug_thread()) {
            printf("  No geometry, returning zero optical depth\n");
            printf("  active_prims.size=%u\n", static_cast<uint>(active_prims.size()));
        }
        // Debug: verify we're actually returning zero
        auto result = make_float3(0.0f);
        if (result.x != 0.0f || result.y != 0.0f || result.z != 0.0f) {
            printf("ERROR: Returning non-zero when no hits! tau=(%.6f,%.6f,%.6f)\n", result.x, result.y, result.z);
        }
        return result;
    }

    // Sort hits by t-value
    sort(hit_buffer);

    // Process all hits and integrate segments
    // Process hits in clusters (all hits at same t-value together)
    float t_prev_hit = 0.0f;

    for (size_t i = 0; i < hit_buffer.size(); ) {
        const float t_current = hit_buffer[i].t_hit;

        // Integrate from previous t to current t
        acc_optical_depth += integrate_primitives(ray, active_prims, t_prev_hit, t_current);

        // Process ALL hits at this t-value
        size_t j = i;
        while (j < hit_buffer.size() && fabsf(hit_buffer[j].t_hit - t_current) < 1e-6f) {
            const auto& hit = hit_buffer[j];
            const auto prim_idx = hit.prim_idx;
            const auto is_exit = hit.is_exit;

            if (is_debug_thread()) {
                printf("  Hit %u: t=%.6f, prim=%u, is_exit=%d\n", static_cast<uint>(j), hit.t_hit, prim_idx, is_exit);
            }

            // Update active primitives set
            if (is_exit) {
                if (!active_prims.contains(prim_idx)) {
                    // Ray started inside this primitive (no entry was detected)
                    // Manually integrate from ray origin to this exit
                    const auto& prim = launch_params.primitives_[prim_idx];
                    acc_optical_depth += prim.density_integral(ray, 0.0f, t_current);

                    if (is_debug_thread()) {
                        printf("  UNPAIRED EXIT: prim %u was active from ray origin, integrated [0,%.3f]\n",
                               prim_idx, t_current);
                    }
                    // Don't try to erase - it's not in the set
                } else {
                    // Normal paired exit - remove from active set
                    if (!active_prims.erase(prim_idx)) {
                        if (is_debug_thread()) printf("erase failed for prim %u\n", prim_idx);
                    } else if (is_debug_thread()) {
                        printf("erased prim %u\n", prim_idx);
                    }
                }
            } else {
                if (!active_prims.insert(prim_idx)) {
                    if (is_debug_thread()) printf("insert failed for prim %u\n", prim_idx);
                } else if (is_debug_thread()) {
                    printf("inserted prim %u\n", prim_idx);
                }
            }

            j++;
        }

        t_prev_hit = t_current;
        i = j;  // Move to next cluster
    }

    // Drain remaining primitives until infinity
    if (is_debug_thread()) {
        printf("  After processing all hits, active_prims: [");
        const auto active_size = active_prims.size();
        for (size_t j = 0; j < active_size; j++) {
            if (j > 0) printf(",");
            printf("%u", *(active_prims.begin() + j));
        }
        printf("] size=%u\n", static_cast<uint>(active_size));
    }

    if (!active_prims.empty()) {
        printf("WARNING: active_prims not empty after processing all hits! size=%u, ray origin=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f)\n",
               static_cast<uint>(active_prims.size()),
               ray.origin_.x, ray.origin_.y, ray.origin_.z,
               ray.direction_.x, ray.direction_.y, ray.direction_.z);
        for (auto prim : active_prims) {
            printf("  - prim %u still active\n", prim);
        }
    }

    // TODO(kacper): this will never happen? remove this?
    acc_optical_depth += integrate_primitives(ray, active_prims, t_prev_hit);

    if (is_debug_thread()) {
        printf("  Final optical depth: (%.3f,%.3f,%.3f)\n",
               acc_optical_depth.x, acc_optical_depth.y, acc_optical_depth.z);
    }

    return acc_optical_depth;
}

} // namespace device
} // namespace thesis
