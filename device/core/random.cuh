#pragma once

#include "thesis/common/utils/math.h"

#include <cstdint>
#include <vector_types.h>

namespace thesis {
namespace device {
namespace random {

// PCG32 — Permuted Congruential Generator (O'Neill 2014)
// 16 bytes vs curandState's ~80 bytes → saves ~16 physical registers
// Quality: passes TestU01 BigCrush and PractRand (full 2^32 period)
// Used by: PBRT v4, Mitsuba 3, Falcor
struct PCG32 {
    uint64_t state_;
    uint64_t inc_;

    __device__ __forceinline__ uint32_t next() {
        const uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        const uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
    }
};

// Mark Jarzynski's PCG hash ("Hash Functions for GPU Rendering", JCGT 2020).
// Scrambles a 64-bit input into a well-distributed 64-bit output — used to
// derive PCG init `sequence` from (pixel, sample) so neighbouring threads get
// decorrelated streams instead of (seq<<1)|1 values clustered along a line.
__device__ __forceinline__ uint64_t hash(uint64_t x) {
    x = x * 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x = x * 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

__device__ __forceinline__ PCG32 init(uint64_t seed, uint64_t sequence) {
    PCG32 rng;
    rng.state_ = 0;
    rng.inc_ = (sequence << 1u) | 1u;  // Increment must be odd
    rng.next();
    rng.state_ += seed;
    rng.next();
    return rng;
}

__device__ __forceinline__ float sample_uniform(PCG32& rng) {
    return static_cast<float>(rng.next()) / static_cast<float>(UINT32_MAX);
}

__device__ __forceinline__ float2 sample_uniform_2d(PCG32& rng, float offset = 0.0f) {
    auto u = make_float2(sample_uniform(rng), sample_uniform(rng));
    return u - offset;
}

}  // namespace random
}  // namespace device
}  // namespace thesis
