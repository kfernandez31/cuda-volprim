#pragma once

#include <cuda_runtime.h>
#include <vector_types.h>

namespace thesis {
namespace device {

struct Camera {
    float3 eye = {};
    float3 pixel00 = {};
    float3 du = {};
    float3 dv = {};
};

} // namespace device 
} // namespace thesis
