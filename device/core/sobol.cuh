#pragma once

#include <cstdint>
#include <vector_types.h>

namespace thesis {
namespace device {
namespace sobol {

// Owen-scrambled Sobol' low-discrepancy points, dimensions 0 and 1.
//
// Used (behind consts::ENABLE_SOBOL_AA) ONLY for the camera anti-aliasing jitter — the
// one sample dimension this path tracer consumes with a FIXED, deterministic index per
// (pixel, sample). The rest of the path (the variable-count per-primitive argmin
// free-flight, NEE/MIS directions drawn after it) has a data-dependent dimension count,
// which Sobol cannot stratify, so those stay on PCG.
//
// Construction: base-2 radical-inverse (dim 0) + the Gray-code Sobol' matrix (dim 1),
// each Owen-scrambled with Brent Burley's practical hash-based nested-uniform scramble
// ("Practical Hash-based Owen Scrambling", JCGT 2020). Per-pixel scramble seeds
// decorrelate the stratified point sets across pixels (turns structured aliasing into
// blue-noise-like error), while preserving low discrepancy along the spp axis per pixel.

__device__ __forceinline__ uint32_t reverse_bits(uint32_t x) {
    return __brev(x);  // hardware bit-reversal
}

// dim 0: van der Corput / radical inverse base 2 = bit-reversed index.
__device__ __forceinline__ uint32_t sobol_dim0(uint32_t index) { return reverse_bits(index); }

// dim 1: Sobol' direction-vector accumulation via the Gray-code recurrence.
__device__ __forceinline__ uint32_t sobol_dim1(uint32_t index) {
    uint32_t v = 0u;
    for (uint32_t d = 0x80000000u; index != 0u; index >>= 1u, d ^= d >> 1u) {
        if (index & 1u)
            v ^= d;
    }
    return v;
}

// Burley 2020 hash-based nested-uniform (Owen) scramble of a 32-bit fixed-point sample.
__device__ __forceinline__ uint32_t owen_scramble(uint32_t x, uint32_t seed) {
    x = reverse_bits(x);
    x ^= x * 0x3d20adeau;
    x += seed;
    x *= (seed >> 16u) | 1u;
    x ^= x * 0x05526c56u;
    x ^= x * 0x53a22864u;
    return reverse_bits(x);
}

// Cheap 32-bit integer hash for deriving per-pixel / per-dimension scramble seeds.
__device__ __forceinline__ uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

__device__ __forceinline__ float to_float_unit(uint32_t x) {
    // [0,1) with 24-bit mantissa precision.
    return (x >> 8) * (1.0f / 16777216.0f);
}

// Owen-scrambled Sobol' 2D point for sample `index` within a pixel, decorrelated per
// pixel via `pixel_seed`. Returns components in [0,1).
__device__ __forceinline__ float2 sample_2d(uint32_t index, uint32_t pixel_seed) {
    const uint32_t s0 = hash_u32(pixel_seed);
    const uint32_t s1 = hash_u32(pixel_seed ^ 0x9e3779b9u);
    const float x = to_float_unit(owen_scramble(sobol_dim0(index), s0));
    const float y = to_float_unit(owen_scramble(sobol_dim1(index), s1));
    return make_float2(x, y);
}

}  // namespace sobol
}  // namespace device
}  // namespace thesis
