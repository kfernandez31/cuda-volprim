#pragma once

#include "thesis/device/params/camera.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/data.h"

#include <cstddef>
#include <glm/glm.hpp>

namespace thesis {
namespace host {

class Camera : public Convertible<device::Camera> {
   private:
    device::Camera device_struct_;

   public:
    size_t image_width_ = 100;
    float aspect_ratio_ = 1.0f;
    float vertical_fov_ = 90.0f;
    glm::vec3 lookfrom_ = glm::vec3(0.0f);
    glm::vec3 lookat_ = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 vup_ = glm::vec3(0.0f, 1.0f, 0.0f);

    Camera() = default;

    Camera(Camera&&) noexcept = default;
    Camera& operator=(Camera&&) noexcept = default;

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    void build() noexcept {
        const auto image_height = glm::max<size_t>(
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

        device_struct_.eye_ = data::toFloat3(lookfrom_);
        device_struct_.pixel00_ = data::toFloat3(pixel00);
        device_struct_.pixel_du_ = data::toFloat3(pixel_du);
        device_struct_.pixel_dv_ = data::toFloat3(pixel_dv);
    }

    [[nodiscard]] device::Camera toDevice() const noexcept override { return device_struct_; }
};

}  // namespace host
}  // namespace thesis
