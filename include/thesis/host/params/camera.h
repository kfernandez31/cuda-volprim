#pragma once

#include "thesis/common/utils/math.h"
#include "thesis/device/params/camera.h"

#include <vector_types.h>

#include <cmath>
#include <cstddef>

namespace thesis {
namespace host {
namespace params {

// Host-side wrapper for camera configuration
class Camera {
   private:
    device::params::Camera device_camera_;

    // Host-only: camera configuration parameters
    size_t image_width_ = 100;
    size_t image_height_ = 100;
    float aspect_ratio_ = 1.0f;
    float vertical_fov_ = 90.0f;
    float3 lookfrom_ = make_float3(0.0f, 0.0f, -1.0f);
    float3 lookat_ = make_float3(0.0f, 0.0f, 0.0f);
    float3 vup_ = make_float3(0.0f, 1.0f, 0.0f);

    void build() noexcept {
        const auto theta = vertical_fov_ * common::math::DEG_TO_RAD_F;
        const auto h = std::tan(0.5f * theta);

        const auto w = common::math::normalize(lookfrom_ - lookat_);
        const auto u = common::math::normalize(common::math::cross(vup_, w));
        const auto v = common::math::cross(w, u);

        const auto focal_len = common::math::length(lookfrom_ - lookat_);
        const auto aspect_ratio =
            static_cast<float>(image_width_) * common::math::rcp(static_cast<float>(image_height_));
        const auto viewport_height = 2.0f * h * focal_len;
        const auto viewport_width = viewport_height * aspect_ratio;

        const auto viewport_u = u * viewport_width;
        const auto viewport_v = -v * viewport_height;

        const auto pixel_du = viewport_u * common::math::rcp(static_cast<float>(image_width_));
        const auto pixel_dv = viewport_v * common::math::rcp(static_cast<float>(image_height_));
        const auto viewport_ul =
            lookfrom_ - focal_len * w - common::math::midpoint(viewport_u, viewport_v);
        const auto pixel00 = viewport_ul + common::math::midpoint(pixel_du, pixel_dv);
        const auto pixel00_relative = pixel00 - lookfrom_;  // Precompute for device

        // Populate device struct with precomputed values
        device_camera_.eye_ = lookfrom_;
        device_camera_.pixel00_relative_ = pixel00_relative;
        device_camera_.pixel_du_ = pixel_du;
        device_camera_.pixel_dv_ = pixel_dv;
    }

   public:
    Camera() = default;

    // Factory method for default camera
    static Camera getDefaultCamera(size_t w, size_t h) noexcept {
        Camera cam;
        cam.image_width_ = w;
        cam.image_height_ = h;
        cam.build();
        return cam;
    }

    // Get device-compatible struct for launch params
    [[nodiscard]] const device::params::Camera& device_camera() const noexcept {
        return device_camera_;
    }

    // Getters for introspection
    [[nodiscard]] float3 lookfrom() const noexcept { return lookfrom_; }
    [[nodiscard]] float3 lookat() const noexcept { return lookat_; }
    [[nodiscard]] float vertical_fov() const noexcept { return vertical_fov_; }
};

}  // namespace params
}  // namespace host
}  // namespace thesis
