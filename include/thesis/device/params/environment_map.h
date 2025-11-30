#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/common/utils/preprocessor.h"

#include <cuda_runtime.h>
#include <vector_types.h>

namespace thesis {
namespace device {
namespace params {

// Device-side POD struct for environment map (no RAII, same size on host and device)
struct THESIS_ALIGNMENT EnvironmentMap {
    cudaTextureObject_t tex_obj_ = 0;

    EnvironmentMap() = default;
    EnvironmentMap(const EnvironmentMap&) = default;
    EnvironmentMap& operator=(const EnvironmentMap&) = default;

#ifdef DEVICE
    // Device-only: sample environment map using hardware-accelerated texture
    __device__ __forceinline__ float3 sample(float3 dir) const {
        namespace math = common::math;

        assert(tex_obj_ != 0);

        // TODO(optimization): This function is called ~2x per bounce, resulting in ~530M
        // calls/frame Current cost: ~55-60 cycles per call (atan2f ~20 cycles, acosf ~30 cycles)
        //
        // Potential optimizations (profile first to confirm this is a bottleneck):
        // 1. Replace acosf with atan2f formulation (33% faster):
        //    theta = atan2(sqrt(x^2 + y^2), z)  vs  theta = acos(z)
        // 2. Use lookup table with bilinear interpolation (10x faster, small memory cost)
        // 3. Use lower resolution env map if aliasing is acceptable

        // Spherical coordinates: theta in [0, pi], phi in [0, 2*pi]
        const auto theta = atan2f(dir.z, dir.x);                  // Polar angle
        const auto phi = acosf(math::clamp(dir.y, -1.0f, 1.0f));  // Azimuthal angle

        // UV coordinates in [0,1] range (normalized coordinates)
        const auto u = math::fma(theta, math::ONE_OVER_TWO_PI_F, 0.5f);
        const auto v = phi * math::ONE_OVER_PI_F;

        // tex2D returns float4 with alpha channel = 1.0f
        const auto rgba = tex2D<float4>(tex_obj_, u, v);
        return make_float3(rgba);
    }
#endif  // DEVICE
};

}  // namespace params
}  // namespace device
}  // namespace thesis
