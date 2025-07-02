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
namespace common {
namespace params {

struct THESIS_ALIGNMENT LaunchParams {
    uint seed_;
    OptixTraversableHandle gas_handle_;
    device::params::Camera camera_;
    device::params::EnvironmentMap env_map_;
    device::params::Image image_;
    device::utils::DynamicVector<device::params::Primitive> primitives_;
    bool debug_;
};

}  // namespace params
}  // namespace common
}  // namespace thesis
