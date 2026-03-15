#pragma once

#include "thesis/common/geometry/quat.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/utils/math.h"

#include <vector_types.h>

namespace thesis {
namespace host {
namespace params {

// Host-side wrapper for primitive
// Thin wrapper around device::params::Primitive — no additional stored state.
// The device primitive stores the CONJUGATE quaternion (world-to-local);
// the forward rotation (needed for OptiX transforms) is derived on demand.
class Primitive {
   private:
    device::params::Primitive device_primitive_;

   public:
    // Default constructor
    Primitive() = default;

    // Constructor: stores conjugate in device_primitive_ for world-to-local transforms
    Primitive(float3 center, const common::geometry::UnitQuaternion& rot_quat, float3 scale,
              float3 albedo, float optical_thickness)
        : device_primitive_(center, rot_quat.conjugate(), scale, albedo, optical_thickness) {}

    // Get device-compatible struct for launch params
    [[nodiscard]] const device::params::Primitive& device_primitive() const noexcept {
        return device_primitive_;
    }

    // Allow non-const access for modification (e.g., albedo clamping in PLY loader)
    [[nodiscard]] device::params::Primitive& device_primitive() noexcept {
        return device_primitive_;
    }

    // Getters for introspection
    [[nodiscard]] float3 center() const { return device_primitive_.center(); }
    [[nodiscard]] common::geometry::UnitQuaternion rot_quat() const {
        return device_primitive_.rot_quat().conjugate();  // Derive forward from stored conjugate
    }
    [[nodiscard]] float3 scale() const { return device_primitive_.scale(); }
    [[nodiscard]] float3 albedo() const { return device_primitive_.albedo_; }
    [[nodiscard]] float optical_thickness() const { return device_primitive_.optical_thickness_; }

    // Generate OptiX transformation matrix (local-to-world, uses forward rotation)
    [[nodiscard]] utils::math::Mat3x4 localToWorld() const noexcept {
        // Scale by Gaussian extent for proper intersection bounds (~3 standard deviations)
        const auto scaled = device_primitive_.scale() * common::math::GAUSSIAN_EXTENT_F;
        const auto forward_rot = device_primitive_.rot_quat().conjugate();
        return utils::math::Mat3x4::from_trs(device_primitive_.center(), forward_rot, scaled);
    }
};

}  // namespace params
}  // namespace host
}  // namespace thesis
