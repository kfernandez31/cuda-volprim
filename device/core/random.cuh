#pragma once

#include "thesis/common/utils/math.h"

#include <curand_kernel.h>
#include <vector_types.h>

namespace thesis {
namespace device {
namespace random {

// Performance note: curand (Philox-based PRNG) costs ~20-30 cycles per sample.
// Sobol quasi-random sequences converge as O(1/N) vs O(1/√N) for pseudo-random,
// yielding 2-4× faster convergence in practice for Monte Carlo integration.
//
// Caveats for switching to Sobol in this renderer:
//   - Each bounce consumes a variable number of random dimensions (depends on
//     primitive count hit by the ray). Sobol works best with fixed-dimension
//     consumption per sample; wasted dimensions degrade toward pseudo-random.
//   - Adaptive sampling skips converged pixels, breaking the lockstep ordering
//     that Sobol assumes. Per-pixel Cranley-Patterson rotation (random offset
//     into a shared Sobol sequence) would be needed.
//   - cuRAND provides built-in Sobol (curandStateScrambledSobol32), so the API
//     change is small — the hard part is the dimension mapping design.

__device__ __forceinline__ curandState init(unsigned long long seed, unsigned long long sequence) {
    curandState state;
    curand_init(seed, sequence, 0, &state);
    return state;
}

__device__ __forceinline__ float sample_uniform(curandState& state) {
    return static_cast<float>(curand(&state)) / static_cast<float>(UINT_MAX);
}

__device__ __forceinline__ float2 sample_uniform_2d(curandState& state, float offset = 0.0f) {
    auto u = make_float2(sample_uniform(state), sample_uniform(state));
    return u - offset;
}

}  // namespace random
}  // namespace device
}  // namespace thesis
