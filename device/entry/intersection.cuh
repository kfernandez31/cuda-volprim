#pragma once

#include <optix.h>
#include "thesis/common/utils/math.h"

namespace thesis {
namespace device {
namespace consts {
constexpr uint ENTRY = 0;
constexpr uint EXIT = 1;
} // namespace consts

extern "C" __global__ void __intersection__sphere() {
    using namespace thesis::device;
    using namespace thesis::common;
    
    printf("intersection called!\n");

    const auto ray = geometry::Ray::getCurrentRay();

    const auto a     = math::length2(ray.direction_);
    const auto a_inv = 1.0f / a;
    const auto b     = -dot(ray.origin_, ray.direction_);

    const auto p = ray.at(b * a_inv);
    const auto delta = 1.0f - math::length2(p);
    if (delta < 0.0f) {
        return; // sphere not along ray
    }

    const auto c = math::length2(ray.origin_) - 1.0f;
    const auto q = b + copysignf(sqrtf(a * delta), b);
    
    const auto t_min = optixGetRayTmin();
    const auto t_2 = q * a_inv;
    if (t_2 < t_min) {
        return; // sphere behind ray
    }

    auto t_1 = c / q;
    if (t_2 - t_1 <= 1e-8f) {
        return; // sphere grazed, ignore
    }

    const auto t_max = optixGetRayTmax();

    // Report entry
    if (t_min <= t_1 && t_1 <= t_max) {
        optixReportIntersection(t_1, consts::ENTRY);
    }

    // Report exit
    if (t_min <= t_2 && t_2 <= t_max) {
        optixReportIntersection(t_2, consts::EXIT);
    }
}

} // namespace device
} // namespace thesis
