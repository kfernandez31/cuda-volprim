#pragma once

#include "core/launch_params.cuh"
#include "core/random.cuh"
#include "core/sampling.cuh"

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/params/ray_state.h"
#include "thesis/device/utils/set.h"
#include "thesis/device/utils/vector.h"

#include <optix.h>
#include <vector_types.h>

#include <assert.h>

// =============================================================================
// Wavefront path tracer — Phase 1 (WAVEFRONT_PLAN.md)
// =============================================================================
// This replaces the megakernel __raygen__rg (device/entry/raygen.cuh) when the build is
// configured with THESIS_WAVEFRONT. It runs EXACTLY ONE bounce per launch: the host drives the
// bounce loop (re-issuing the SAME optixLaunch max_bounces times) over a global RayState[N]
// (N = width × height × samples_in_batch). NO kernel split and NO stream compaction yet — every
// ray's thread runs every bounce launch; dead/finished rays early-out on RayState::bounce_.
//
// The per-ray bounce index lives in RayState (not a launch param), so launch params are uploaded
// ONCE per batch — re-uploading them per bounce from pageable host memory stalled ~60 ms/bounce.
//
// Purpose: measure the cost of streaming RayState (esp. the ~264 B active_prims CompactSet) to
// global memory every bounce — the #1 risk (R1) of the whole wavefront bet — for the price of a
// <2-day probe. The per-bounce math is the megakernel's body, lifted verbatim, so renders must
// match the megakernel up to fast-math FMA reassociation (the same ~1-ULP class the runtime
// RenderParams change already accepted; see launch_params.h). Welford accumulation across the
// batch's samples is deferred to a separate finalize kernel (device/kernels/wavefront_finalize).

namespace thesis {
namespace device {

extern "C" __global__ void __raygen__rg() {
    using namespace thesis::device;
    namespace math = thesis::common::math;

    const auto launch_idx = optixGetLaunchIndex();

    const auto pixel_idx = make_uint2(launch_idx.x, launch_idx.y);
    // Widen to size_t so width × height ≥ 2^32 (8K+) doesn't overflow uint32 arithmetic.
    const size_t num_pixels =
        static_cast<size_t>(launch_params.image_.width_) * launch_params.image_.height_;
    const size_t pixel_linear_idx =
        static_cast<size_t>(launch_idx.y) * launch_params.image_.width_ + launch_idx.x;
    const size_t sample_in_batch = launch_idx.z;
    // Layout: rays for a fixed sample are contiguous over pixels, so adjacent warp lanes
    // (adjacent x) touch adjacent RayState — coalesced streaming each bounce.
    const size_t ray_idx = sample_in_batch * num_pixels + pixel_linear_idx;

    auto& state = launch_params.ray_states_[ray_idx];
    const uint32_t bounce = state.bounce_;

    // Finished path: nothing more to do (cheap leading-word read).
    if (bounce == params::WF_RAY_DEAD) {
        return;
    }

    random::PCG32 rng;
    float3 throughput;
    float3 radiance;
    float3 ray_origin;
    float3 ray_direction;
    float3 sample_aov_albedo = make_float3(0.0f);
    float3 sample_aov_normal = make_float3(0.0f);

    optix::ScatteringEvent<PrimsSet> event;
    payloads::Miss miss;
    HitBuffer hit_buffer;

    if (bounce == 0) {
        // ── INIT (first launch of the batch): identical RNG / ray / throughput setup to the
        // megakernel. RayState was memset to 0 for this batch, so bounce_ == 0 ⇔ uninitialized. ──
        const size_t global_sample_idx = launch_params.image_.batch_offset_ + sample_in_batch;
        const uint64_t rng_seed = random::hash(
            pixel_linear_idx * launch_params.image_.num_samples_per_pixel_ + global_sample_idx);
        rng = random::init(launch_params.seed_, rng_seed);

        float2 jitter;
        if (launch_params.render_.pixel_filter_stddev_ > 0.0f) {
            jitter = random::sample_gaussian_2d(rng, launch_params.render_.pixel_filter_stddev_);
        } else {
            jitter = random::sample_uniform_2d(rng, 0.5f);
        }
        const auto cam_ray = launch_params.camera_.jittered_ray(pixel_idx, jitter);
        ray_origin = cam_ray.origin_;
        ray_direction = cam_ray.direction_;

        throughput = make_float3(1.0f);
        radiance = make_float3(0.0f);
    } else {
        // ── Resume a live path. ──
        rng = random::PCG32{state.rng_state_, state.rng_inc_};
        throughput = state.throughput_;
        radiance = state.radiance_;
        ray_origin = state.origin_;
        ray_direction = state.direction_;
        // sample_scattering_event(first_bounce=false) requires active_prims_ to already hold
        // the origin-inside set (= the previous scatter point's active set). In the megakernel
        // this is carried in the persistent `event` local; here it lives in RayState.
        event.active_prims_ = state.active_prims_;
    }

    const auto ray = geometry::Ray::spawn_unchecked(ray_origin, ray_direction);

    // Helper to persist bounce-0 AOV (written at bounce 0 only, regardless of terminate/continue).
    auto store_bounce0_aov = [&]() {
        if (bounce == 0 && launch_params.image_.albedo_aov_) {
            state.aov_albedo_ = sample_aov_albedo;
            state.aov_normal_ = sample_aov_normal;
        }
    };

    // ── One bounce body (lifted verbatim from the megakernel loop) ──
    PrimsSet camera_origin_inside;
    const auto result = sample_scattering_event(ray, rng, event, miss, hit_buffer,
                                                /*first_bounce=*/bounce == 0,
                                                bounce == 0 ? &camera_origin_inside : nullptr);

    if (bounce == 0) {
        sample_aov_normal = -ray.direction_;
        if (result) {
            sample_aov_albedo = evaluate_albedo(event.position_, event.active_prims_);
        }
    }

    if constexpr (consts::ENABLE_ANALYTIC_DIRECT) {
        if (bounce == 0) {
            const auto T_dir = compute_transmittance_to_env(ray.origin_, ray.direction_,
                                                            camera_origin_inside);
            radiance += throughput * T_dir * launch_params.env_map_.sample(ray.direction_);
        }
    }

    // Escape (no scatter). See megakernel raygen.cuh for the full double-count reasoning.
    if (!result) {
        if constexpr (consts::ENABLE_NEE) {
            if constexpr (!consts::ENABLE_ANALYTIC_DIRECT) {
                if (bounce == 0) {
                    radiance += throughput * miss.color();
                }
            }
        } else {
            if constexpr (consts::ENABLE_ANALYTIC_DIRECT) {
                if (bounce > 0) {
                    radiance += throughput * miss.color();
                }
            } else {
                radiance += throughput * miss.color();
            }
        }
        // Path done: persist radiance (+ bounce-0 AOV) and mark finished.
        state.radiance_ = radiance;
        store_bounce0_aov();
        state.bounce_ = params::WF_RAY_DEAD;
        return;
    }

    const auto albedo = evaluate_albedo(event.position_, event.active_prims_);

    if constexpr (consts::ENABLE_NEE) {
        const auto wi = ray.direction_;
        const auto base = throughput * albedo;

        if constexpr (consts::ENABLE_MIS) {
            // ─── Strategy A: phase importance sampling ───
            const auto a = phase::sample(wi, rng);
            const auto pdf_b_at_a = env_is::pdf(a.wo);
            const auto w_a = mis_balance(a.pdf, pdf_b_at_a);
            const auto env_a = launch_params.env_map_.sample(a.wo);
            const auto T_a =
                compute_transmittance_to_env(event.position_, a.wo, event.active_prims_);
            radiance += base * env_a * T_a * w_a;

            // ─── Strategy B: environment importance sampling ───
            const auto b = env_is::sample(rng);
            const auto phase_at_b = phase::eval(wi, b.wo);
            const auto w_b = mis_balance(b.pdf, phase_at_b);
            const auto env_b = launch_params.env_map_.sample(b.wo);
            const auto T_b =
                compute_transmittance_to_env(event.position_, b.wo, event.active_prims_);
            radiance += base * env_b * T_b * w_b * phase_at_b * math::rcp(b.pdf);
        } else {
            const auto sample = phase::sample(wi, rng);
            const auto env = launch_params.env_map_.sample(sample.wo);
            const auto T =
                compute_transmittance_to_env(event.position_, sample.wo, event.active_prims_);
            radiance += base * env * T;
        }
    } else {
        const auto env = launch_params.env_map_.sample(event.direction_);
        radiance += throughput * albedo * env;
    }
    throughput *= albedo;

    // Russian Roulette
    if (bounce >= launch_params.render_.rr_depth_) {
        auto p_survive = math::min(launch_params.render_.rr_max_survival_, math::max(throughput));
        if (random::sample_uniform(rng) > p_survive) {
            state.radiance_ = radiance;
            store_bounce0_aov();
            state.bounce_ = params::WF_RAY_DEAD;
            return;
        }
        throughput /= p_survive;
    }

    // Early termination: throughput too low / non-finite.
    if (math::max(throughput) < consts::MIN_THROUGHPUT || !isfinite(math::sum(throughput))) {
        state.radiance_ = radiance;
        store_bounce0_aov();
        state.bounce_ = params::WF_RAY_DEAD;
        return;
    }

    // ── Continue: persist full state for the next bounce launch ──
    const auto next_ray = geometry::Ray::spawn_unchecked(event.position_, event.direction_);
    state.origin_ = next_ray.origin_;
    state.direction_ = next_ray.direction_;
    state.throughput_ = throughput;
    state.radiance_ = radiance;
    state.rng_state_ = rng.state_;
    state.rng_inc_ = rng.inc_;
    state.active_prims_ = event.active_prims_;
    store_bounce0_aov();
    state.bounce_ = bounce + 1;
}

}  // namespace device
}  // namespace thesis
