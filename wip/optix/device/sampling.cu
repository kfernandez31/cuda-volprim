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

// chance for any point is 1 over integral of surface
__forceinline__ __device__ void phase_value() {
    return math::ONE_OVER_FOUR_PI_F;
}

__forceinline__ __device__ float3 sample_phase(float3 /*wi*/, float2 sample) {
    // Isotropic phase function: uniform over sphere
    // Role:
    // Determines in which direction light scatters after the event.

    // Mechanism:
    // Draws a new direction from a phase function, which is a PDF over the unit sphere. Controls anisotropy of scattering.
    auto z = 1.0f - 2.0f * sample.x;
    auto r = sqrtf(fmaxf(0.0f, 1.0f - math::pow2(z)));
    auto phi = math::TWO_PI_F * sample.y;
    return make_float3(r * cosf(phi), r * sinf(phi), z); // direction
}

__device__ __forceinline__ float sample_target_optical_depth(float uniform_sample) {
    // Inverse transform sampling from exponential distribution:
    // PDF:   p(τ) = e^(-τ)
    // CDF:   F(τ) = 1 - e^(-τ)
    // Inverse CDF: τ = -ln(1 - ξ), ξ ∈ [0,1)

    // Clamp to avoid log(0), which would be infinite
    return -logf(fmaxf(1.0f - uniform_sample, 1e-6f));
}

// TODO(kacper) potential to optimize: don't invert, select random position on the segment which we call the ...
__device__ float sample_distance_bisection(
    const Ray& ray,
    float2 t_range,
    float sample,
    Set<unsigned int, MAX_HITS>& prim_indices
) {
    constexpr size_t MAX_ITER = 24;
    constexpr float EPS = 1e-4f;

    const float target_tau = -logf(fmaxf(1.0f - sample, 1e-6f));
    float t_lower = t_range.x;
    float t_upper = t_range.y;

    for (size_t iter = 0; iter < MAX_ITER; ++iter) {
        const auto t_mid = 0.5f * (t_lower + t_upper);
        const auto tau = optical_depth_accumulated(ray, t_mid, prim_indices);
        const auto error = tau - target_tau;

        if (fabsf(error) < EPS)
            return t_mid;

        if (error > 0.0f)
            t_upper = t_mid;  // We overestimated τ → reduce upper bound
        else
            t_lower = t_mid;  // We underestimated τ → increase lower bound
    }

    // Final approximation after max iterations
    return 0.5f * (t_lower + t_upper);
}
