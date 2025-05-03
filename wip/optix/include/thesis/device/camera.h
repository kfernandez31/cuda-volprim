#pragma once

#include <cuda_runtime.h>
#include <vector_types.h>

namespace thesis {
namespace device {

struct Camera {
    float3 eye_ = {};
    float3 pixel00_ = {};
    float3 pixel_du_ = {};
    float3 pixel_dv_ = {};
};

}  // namespace device
}  // namespace thesis
