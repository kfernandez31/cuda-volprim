#pragma once

#include <vector_types.h>

#include <cstddef>
#include <cstdint>

namespace thesis {
namespace device {
namespace params {
struct Primitive;
}  // namespace params

namespace kernels {

// Number of uint32_t elements the camera-active-set buffer must hold
// (2 header slots + MAX_ACTIVE_PRIMS indices; layout documented in the .cu).
extern "C" std::size_t camera_active_set_buffer_len();

// Compute the origin-inside (bounce-0) primitive set for a shared-origin camera, once
// per render, into `out` on `stream`. Single-thread kernel by design: the ascending
// visit order must match the in-megakernel scan's insertion order exactly (the RNG draw
// order downstream follows set order and is load-bearing for bit-exactness).
extern "C" void launch_camera_active_set_kernel(const thesis::device::params::Primitive* prims,
                                                std::size_t num_primitives, float3 origin,
                                                std::uint32_t* out, cudaStream_t stream);

}  // namespace kernels
}  // namespace device
}  // namespace thesis
