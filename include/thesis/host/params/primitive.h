#pragma once

#include "thesis/common/geometry/quat.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/utils/math.h"

#include <vector_types.h>

namespace thesis {
namespace host {
namespace params {

// Host-side wrapper for primitive
class Primitive {
   private:
    device::params::Primitive device_primitive_;

   public:
    // Default constructor
    Primitive() = default;

    // Constructor with UnitQuaternion
    Primitive(float3 center, const common::geometry::UnitQuaternion& rot_quat, float3 scale,
              float3 albedo, float optical_thickness)
        : device_primitive_(center, rot_quat, scale, albedo, optical_thickness) {}

    // Get device-compatible struct for launch params
    [[nodiscard]] const device::params::Primitive& device_primitive() const noexcept {
        return device_primitive_;
    }

    // Allow non-const access for modification
    [[nodiscard]] device::params::Primitive& device_primitive() noexcept {
        return device_primitive_;
    }

    // Getters for introspection
    [[nodiscard]] float3 center() const { return device_primitive_.center(); }
    [[nodiscard]] const common::geometry::UnitQuaternion& rot_quat() const {
        return device_primitive_.rot_quat();
    }
    [[nodiscard]] float3 scale() const { return device_primitive_.scale(); }
    [[nodiscard]] float3 albedo() const { return device_primitive_.albedo_; }
    [[nodiscard]] float optical_thickness() const { return device_primitive_.optical_thickness_; }

    // Generate OptiX transformation matrix
    [[nodiscard]] utils::math::Mat3x4 localToWorld() const noexcept {
        static constexpr auto INTERSECTION_SCALING_FACTOR = 1.0f;  // TODO(kacper): set to 3.0f
        const auto scaled = device_primitive_.scale() * INTERSECTION_SCALING_FACTOR;
        return utils::math::Mat3x4::from_trs(device_primitive_.center(),
                                             device_primitive_.rot_quat(), scaled);
    }
};

}  // namespace params
}  // namespace host
}  // namespace thesis
