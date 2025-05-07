#pragma once

#include "thesis/device/camera.h"
#include "thesis/utils/vec.h"

#include <cstddef>
#include <glm/glm.hpp>

namespace thesis {
namespace host {

class Camera {
   private:
    device::Camera device_struct;

   public:
    size_t image_width_ = 100;
    float aspect_ratio_ = 1.0f;
    float vertical_fov_ = 90.0f;
    glm::vec3 lookfrom_ = glm::vec3(0.0f);
    glm::vec3 lookat_ = glm::vec3(0.0f, 0.0f, -1.0f); // TODO(kacper): why isn't it used in the device?
    glm::vec3 vup_ = glm::vec3(0.0f, 1.0f, 0.0f);

    Camera() = default;

    Camera(Camera&& other) = default;
    Camera& operator=(Camera&& other) = default;

    Camera(const Camera& other) = delete;
    Camera& operator=(const Camera& other) = delete;

    void build() {
        const auto image_height = std::max<size_t>(
            1, static_cast<size_t>(static_cast<float>(image_width_) / aspect_ratio_));

        const auto theta = glm::radians(vertical_fov_);
        const auto h = glm::tan(0.5f * theta);

        const auto w = glm::normalize(lookfrom_ - lookat_);
        const auto u = glm::normalize(glm::cross(vup_, w));
        const auto v = glm::cross(w, u);

        const auto focal_len = glm::length(lookfrom_ - lookat_);
        const auto viewport_height = 2.0f * h * focal_len;
        const auto viewport_width = viewport_height * aspect_ratio_;

        const auto viewport_u = u * viewport_width;
        const auto viewport_v = -v * viewport_height;

        const auto pixel_du = viewport_u / static_cast<float>(image_width_);
        const auto pixel_dv = viewport_v / static_cast<float>(image_height);
        const auto viewport_ul = lookfrom_ - focal_len * w - 0.5f * (viewport_u + viewport_v);
        const auto pixel00 = viewport_ul + 0.5f * (pixel_du + pixel_dv);
        
        device_struct.eye_       = to_float3(lookfrom_);
        device_struct.pixel00_   = to_float3(pixel00);
        device_struct.pixel_du_  = to_float3(pixel_du);
        device_struct.pixel_dv_  = to_float3(pixel_dv);
    }

    [[nodiscard]] device::Camera toDevice() const noexcept { return device_struct; }
};

}  // namespace host
}  // namespace thesis
