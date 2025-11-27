#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"

#include <cuda_runtime.h>
#include <vector_types.h>

#include <cstddef>
#include <math.h>
#include <sutil/vec_math.h>

namespace thesis {
namespace device {
namespace params {

struct THESIS_ALIGNMENT EnvironmentMap {
    cudaTextureObject_t tex_obj_ = 0;

#ifdef DEVICE
    // Sample environment map using hardware-accelerated texture (bilinear interpolation)
    __device__ float3 sample(float3 dir) const {
        namespace math = common::math;

        assert(tex_obj_ != 0);

        // TODO(optimization): This function is called ~2x per bounce, resulting in ~530M
        // calls/frame Current cost: ~55-60 cycles per call (atan2f ~20 cycles, acosf ~30 cycles)
        //
        // Potential optimizations (profile first to confirm this is a bottleneck):
        // 1. Replace acosf with atan2f formulation (33% faster):
        //    const float r_xz = sqrtf(fmaf(dir.x, dir.x, dir.z * dir.z));
        //    const float phi = atan2f(r_xz, dir.y);
        //
        // 2. Use explicit FMA for UV calculation:
        //    const float u = fmaf(theta, math::ONE_OVER_TWO_PI_F, 0.5f);
        //
        // 3. Use fast atan2 approximation for 5x speedup (~12 cycles total):
        //    Fast polynomial approximation with error < 0.005 radians
        //    Usually imperceptible for environment lighting
        //
        // Expected impact: 60 cycles → 20 cycles (conservative) or 12 cycles (aggressive)

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
