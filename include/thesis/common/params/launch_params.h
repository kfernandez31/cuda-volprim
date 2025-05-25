#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/params/camera.h"
#include "thesis/device/params/environment_map.h"
#include "thesis/device/params/image.h"
#include "thesis/device/params/primitive.h"
#include "thesis/device/utils/vector.h"

#include <optix.h>
#include <vector_types.h>

#include <cstddef>

namespace thesis {
namespace optix {

struct THESIS_ALIGNMENT LaunchParams {
    OptixTraversableHandle gas_handle_;
    size_t num_triangles_per_primitive_;
    uint seed_;
    device::Image image_;
    device::EnvironmentMap env_map_;
    device::Camera camera_;
    device::utils::DynamicVector<device::Primitive> primitives_;
};

struct THESIS_ALIGNMENT RayGenData {
    // No data needed
};

struct THESIS_ALIGNMENT MissData {
    // No data needed
};

struct THESIS_ALIGNMENT HitGroupData {
    // No data needed
};

}  // namespace optix
}  // namespace thesis
