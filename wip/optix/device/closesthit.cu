#include "thesis/device/primitive.h"
#include "thesis/device/ray.h"
#include "thesis/optix/launch_params.h"

#include <optix.h>

#include <sutil/vec_math.h>

#include "common.cuh"

extern "C" __global__ void __closesthit__ch() {
    const auto triangle_idx = optixGetPrimitiveIndex();
    const auto prim_idx = triangle_idx / params.num_triangles_per_primitive_;

    const auto ray = thesis::device::Ray::getCurrentRay();

    const auto& prim = params.primitives_[prim_idx];
    const auto color = prim.density_integral(ray);

    setPayload(color);
}
