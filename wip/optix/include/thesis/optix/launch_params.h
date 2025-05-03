#pragma once

// TODO: _ case

#include "thesis/device/camera.h"
#include "thesis/device/environment_map.h"
#include "thesis/device/image.h"

#include <optix.h>
#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace optix {

enum RayType {
    RAY_TYPE_RADIANCE = 0,
    RAY_TYPE_COUNT,
};

__align__(16) struct LaunchParams {
    OptixTraversableHandle handle_;
    size_t num_samples_per_pixel_;
    device::Image image_;
    device::EnvironmentMap env_map_;
    device::Camera camera_;
};

struct RayGenData {
    // No data needed
};

struct MissData {
    // No data needed
};

struct HitGroupData {
    // No data needed
};

}  // namespace optix
}  // namespace thesis
