#pragma once

#include <vector_types.h>
#include <curand_kernel.h>
#include <sutil/vec_math.h>

namespace thesis {
namespace device {
namespace random {

__device__ __forceinline__ float sample_uniform(curandState& state) {
    return static_cast<float>(curand(&state)) / static_cast<float>(UINT_MAX);
}

__device__ __forceinline__ float2 sample_uniform_2d(curandState& state, float offset=0.0f) {
    auto u = make_float2(sample_uniform(state), sample_uniform(state));
    return u - offset;
}

} // namespace random
} // namespace device
} // namespace thesis
