#pragma once

#include "thesis/common/utils/math.h"

#include <curand_kernel.h>
#include <vector_types.h>

namespace thesis {
namespace device {
namespace random {

// TODO: rethink whether I want to use curand and not e.g. Sobol

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
