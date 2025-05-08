#pragma once

#include <vector_types.h>

namespace thesis {
namespace device {

struct alignas(16) Camera {
    float3 eye_ = {};
    float3 pixel00_ = {};
    float3 pixel_du_ = {};
    float3 pixel_dv_ = {};
};

}  // namespace device
}  // namespace thesis
