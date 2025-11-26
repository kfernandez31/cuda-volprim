#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"
#include <sutil/vec_math.h>

#include <cuda_runtime.h>
#include <vector_types.h>

#include <cstddef>
#include <math.h>

namespace thesis {
namespace device {
namespace params {

struct THESIS_ALIGNMENT EnvironmentMap {
    cudaTextureObject_t tex_obj_ = 0;

#ifdef DEVICE
    /// Sample environment map using hardware-accelerated texture (bilinear interpolation)
    __device__ float3 sample(float3 dir) const {
        namespace math = common::math;

        assert(tex_obj_ != 0);

        // Convert direction to spherical coordinates
        const auto theta = atan2f(dir.z, dir.x);
        const auto phi = acosf(math::clamp(dir.y, -1.0f, 1.0f));

        // UV coordinates in [0,1] range (normalized coordinates)
        const auto u = (theta + math::PI_F) * math::ONE_OVER_TWO_PI_F;
        const auto v = phi * math::ONE_OVER_PI_F;

        // Hardware-filtered texture lookup (bilinear interpolation automatic!)
        const auto rgba = tex2D<float4>(tex_obj_, u, v);
        return make_float3(rgba);
    }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
