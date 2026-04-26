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
// Note: image dimensions are NOT stored here — they belong to Image.
// The camera only needs them transiently during build() to compute pixel deltas.
class Camera {
   private:
    device::params::Camera device_camera_;

    // Host-only: camera configuration parameters
    float vertical_fov_ = 90.0f;
    float3 lookfrom_ = make_float3(0.0f, 0.0f, -1.0f);
    float3 lookat_ = make_float3(0.0f, 0.0f, 0.0f);
    float3 vup_ = make_float3(0.0f, 1.0f, 0.0f);

    // Orthographic-specific parameters
    bool is_orthographic_ = false;
    float ortho_height_ = 2.0f;  // Height of orthographic viewport

    void build(size_t image_width, size_t image_height) noexcept {
        if (is_orthographic_) {
            buildOrthographic(image_width, image_height);
        } else {
            buildPerspective(image_width, image_height);
        }
    }

    void buildPerspective(size_t image_width, size_t image_height) noexcept {
        const auto theta = vertical_fov_ * common::math::DEG_TO_RAD_F;
        const auto h = std::tan(0.5f * theta);

        const auto w = common::math::normalize(lookfrom_ - lookat_);
        const auto u = common::math::normalize(common::math::cross(vup_, w));
        const auto v = common::math::cross(w, u);

        const auto focal_len = common::math::length(lookfrom_ - lookat_);
        const auto aspect_ratio =
            static_cast<float>(image_width) * common::math::rcp(static_cast<float>(image_height));
        const auto viewport_height = 2.0f * h * focal_len;
        const auto viewport_width = viewport_height * aspect_ratio;

        const auto viewport_u = u * viewport_width;
        const auto viewport_v = -v * viewport_height;

        const auto pixel_du = viewport_u * common::math::rcp(static_cast<float>(image_width));
        const auto pixel_dv = viewport_v * common::math::rcp(static_cast<float>(image_height));
        const auto viewport_ul =
            lookfrom_ - focal_len * w - common::math::midpoint(viewport_u, viewport_v);
        const auto pixel00 = viewport_ul + common::math::midpoint(pixel_du, pixel_dv);
        const auto pixel00_relative = pixel00 - lookfrom_;  // Precompute for device

        // Populate device struct with precomputed values
        device_camera_.is_orthographic_ = false;
        device_camera_.eye_ = lookfrom_;
        device_camera_.pixel00_relative_ = pixel00_relative;
        device_camera_.pixel_du_ = pixel_du;
        device_camera_.pixel_dv_ = pixel_dv;
    }

    void buildOrthographic(size_t image_width, size_t image_height) noexcept {
        // Compute camera basis vectors
        const auto view_dir = common::math::normalize(lookat_ - lookfrom_);
        const auto right = common::math::normalize(common::math::cross(view_dir, vup_));
        const auto up = common::math::cross(right, view_dir);

        // Orthographic viewport dimensions
        const auto aspect_ratio =
            static_cast<float>(image_width) * common::math::rcp(static_cast<float>(image_height));
        const auto viewport_height = ortho_height_;
        const auto viewport_width = viewport_height * aspect_ratio;

        // Viewport vectors (in world space)
        // Note: positive up (not negated) because saveExr applies flip_vertical=true,
        // converting from bottom-up GPU convention to top-down EXR convention.
        const auto viewport_u = right * viewport_width;
        const auto viewport_v = up * viewport_height;

        // Pixel delta vectors
        const auto pixel_du = viewport_u * common::math::rcp(static_cast<float>(image_width));
        const auto pixel_dv = viewport_v * common::math::rcp(static_cast<float>(image_height));

        // View plane center (eye position for orthographic reference point)
        const auto view_center = lookfrom_;

        // Upper-left corner of viewport
        const auto viewport_ul = view_center - common::math::midpoint(viewport_u, viewport_v);

        // Pixel (0,0) center position
        const auto pixel00 = viewport_ul + common::math::midpoint(pixel_du, pixel_dv);
        const auto pixel00_relative = pixel00 - view_center;

        // Populate device struct
        device_camera_.is_orthographic_ = true;
        device_camera_.eye_ = view_center;
        device_camera_.pixel00_relative_ = pixel00_relative;
        device_camera_.pixel_du_ = pixel_du;
        device_camera_.pixel_dv_ = pixel_dv;
        device_camera_.view_direction_ = view_dir;
    }

   public:
    Camera() = default;

    // Factory method for default camera
    static Camera getDefaultCamera(size_t w, size_t h) noexcept {
        Camera cam;
        cam.is_orthographic_ = false;
        cam.build(w, h);
        return cam;
    }

    // Factory method for orthographic camera
    static Camera createOrthographic(size_t width, size_t height, float3 origin, float3 target,
                                     float3 up, float ortho_height) noexcept {
        Camera cam;
        cam.is_orthographic_ = true;
        cam.lookfrom_ = origin;
        cam.lookat_ = target;
        cam.vup_ = up;
        cam.ortho_height_ = ortho_height;
        cam.build(width, height);
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
    [[nodiscard]] bool is_orthographic() const noexcept { return is_orthographic_; }
};

}  // namespace params
}  // namespace host
}  // namespace thesis
