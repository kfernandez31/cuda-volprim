#pragma once

#include "thesis/optix/launch_params.h"
#include "thesis/device/vector.h"
#include "thesis/device/set.h"
#include "thesis/utils/math.h"
#include "thesis/utils/vec_math.h"

#include <optix.h>
#include <vector_types.h>

#include "random.cuh"
#include "trace.cuh"

// TODO(kacper): good practice or remove?
namespace {

constexpr auto MAX_HITS           = 64u; // TODO(kacper): what to pick?
constexpr auto MAX_BOUNCES         = 64u;
constexpr auto RUSSIAN_ROULETTE_DEPTH = 3u;
constexpr auto MIN_THROUGHPUT      = 1e-3f;
constexpr auto RR_MAX_SURVIVAL     = 0.99f;
constexpr auto ESCAPE_CLAMP_EPS    = 1e-6f;

} // namespace

// TODO(kacper): ugly
using namespace thesis::device;

extern "C" __global__ void __raygen__rg() {
    const auto launch_idx = optixGetLaunchIndex();
    uint2 pixel = make_uint2(launch_idx.x % width, launch_idx.y);
    uint  sample_idx = launch_idx.z / width;

    // initialize rng
    curandState rng;
    curand_init(params.seed, pixel.y * params.image_width + pixel.x, sample, &rng); // TODO(kacper): correct indexing to take into account 3D

    const auto jitter = random::sample_uniform_2d(&rng, 0.5f);
    Ray ray = random::compute_jittered_ray(jitter, pixel.idx);

    auto throughput = make_float3(1.0f);
    auto radiance = make_float3(0.0f);

    for (size_t bounce = 0; bounce < MAX_BOUNCES; ++bounce) {
        auto evt = sample_scattering_event(ray, &rng);

        // no scattering - escaped medium
        if (!evt) {
            Set<unsigned int, MAX_HITS> active_prims;
            auto tau = compute_optical_depth_along_ray(ray);
            auto env = params.env_map_.sample(ray.direction_);
            radiance += throughput * expf(-tau) * env;
            break;
        }

        // Evaluate albedo and environment lighting
        auto albedo = evaluate_albedo(evt->position, evt->active_prims);
        auto env = params.env_map_.sample(evt->direction);
        radiance += throughput * albedo * env * phase_value();

        // Update energy by scattered amount (albedo)
        throughput *= albedo;

        // Russian Roulette
        if (bounce >= RUSSIAN_ROULETTE_DEPTH) {
            float p_survive = fminf(RR_MAX_SURVIVAL, max(throughput));
            if (random::sample_uniform(&rng) > p_survive)
                break;
            throughput /= p_survive;
        }

        // Prepare next ray
        ray = Ray::spawn(evt->position, evt->direction);

        if (!isfinite(sum(throughput)) || max(throughput) < MIN_THROUGHPUT)
            break;
    }

    radiance /= static_cast<float>(params.num_samples_per_pixel_);
    params.image_(idx.x, idx.y) = acc_radiance;
}
