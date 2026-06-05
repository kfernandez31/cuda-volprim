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
    OptixTraversableHandle ias_handle_;
    device::params::Camera camera_;
    device::params::EnvironmentMap env_map_;
    device::params::Image image_;
    device::utils::DynamicVector<device::params::Primitive> primitives_;
    uint seed_;

    // Device-side atomic counter for cap-overflow events (CompactSet active-prims or
    // the primary-ray HitBuffer dropping an entry). Bumped from device code, read back
    // by the host after the render to WARN that dense-overlap regions may be biased
    // (under-absorption) — turning the previously SILENT overflow into a visible signal.
    // Single element; null-guarded on device.
    unsigned long long* overflow_counter_ = nullptr;
};

}  // namespace params
}  // namespace common
}  // namespace thesis
