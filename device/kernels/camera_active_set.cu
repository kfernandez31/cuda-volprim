#include "kernels/camera_active_set.h"

#include "core/constants.cuh"
#include "thesis/device/params/primitive.h"

#include "thesis/common/geometry/intersection.h"  // needs the full Primitive type above

#include <vector_types.h>

#include <cstddef>
#include <cstdint>

namespace thesis {
namespace device {
namespace kernels {

// Buffer layout: out[0] = count, out[1] = inserts refused at MAX_ACTIVE_PRIMS (the
// would-be OVERFLOW_ACTIVE_SET events), out[2 .. 2+count) = primitive indices, ascending.
//
// Every perspective camera ray shares one origin, so the origin-inside set that
// build_origin_inside_set (device/core/sampling.cuh) scans all N primitives for — once
// per sample — is a per-launch constant. This kernel computes it once per render with
// the SAME device predicate, so the set (and the RNG draw order that follows it) is
// identical to the in-kernel scan's.
//
// Single thread on purpose: N is at most a few tens of thousands and this runs once per
// render; a parallel version would need a sort to reproduce the ascending order.
static __global__ void camera_active_set_kernel(const params::Primitive* prims,
                                                size_t num_primitives, float3 origin,
                                                uint32_t* out) {
    uint32_t count = 0;
    uint32_t refused = 0;
    for (size_t i = 0; i < num_primitives; ++i) {
        if (common::geometry::point_inside_bvh_bound(origin, prims[i])) {
            if (count < consts::MAX_ACTIVE_PRIMS) {
                out[2 + count] = static_cast<uint32_t>(i);
                ++count;
            } else {
                ++refused;
            }
        }
    }
    out[0] = count;
    out[1] = refused;
}

extern "C" size_t camera_active_set_buffer_len() { return 2 + consts::MAX_ACTIVE_PRIMS; }

extern "C" void launch_camera_active_set_kernel(const params::Primitive* prims,
                                                size_t num_primitives, float3 origin,
                                                uint32_t* out, cudaStream_t stream) {
    camera_active_set_kernel<<<1, 1, 0, stream>>>(prims, num_primitives, origin, out);
}

}  // namespace kernels
}  // namespace device
}  // namespace thesis
