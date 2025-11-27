#pragma once

#include "thesis/device/params/camera.h"
#include "thesis/host/params/convertible.h"
#include "thesis/host/utils/data.h"

#include <cstddef>
#include <glm/glm.hpp>

namespace thesis::host::params {

class Camera : public Convertible<device::params::Camera> {
   private:
    device::params::Camera device_struct_;

   public:
    size_t image_width_ = 100;
    size_t image_height_ = 100;
    float aspect_ratio_ = 1.0f;
    float vertical_fov_ = 90.0f;
    glm::vec3 lookfrom_ = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 lookat_ = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 vup_ = glm::vec3(0.0f, 1.0f, 0.0f);

    static Camera getDefaultCamera(size_t w, size_t h) noexcept {
        Camera cam;
        cam.image_width_ = w;
        cam.image_height_ = h;
        cam.build();
        return cam;
    }

    Camera() = default;

    Camera(Camera&&) noexcept = default;
    Camera& operator=(Camera&&) noexcept = default;

    Camera(const Camera&) = default;
    Camera& operator=(const Camera&) = default;

    [[nodiscard]] device::params::Camera toDevice() const noexcept override {
        return device_struct_;
    }

   private:
    void build() noexcept {
        const auto theta = glm::radians(vertical_fov_);
        const auto h = glm::tan(0.5f * theta);

        const auto w = glm::normalize(lookfrom_ - lookat_);
        const auto u = glm::normalize(glm::cross(vup_, w));
        const auto v = glm::cross(w, u);

        const auto focal_len = glm::length(lookfrom_ - lookat_);
        const auto aspect_ratio =
            static_cast<float>(image_width_) / static_cast<float>(image_height_);
        const auto viewport_height = 2.0f * h * focal_len;
        const auto viewport_width = viewport_height * aspect_ratio;

        const auto viewport_u = u * viewport_width;
        const auto viewport_v = -v * viewport_height;

        const auto pixel_du = viewport_u / static_cast<float>(image_width_);
        const auto pixel_dv = viewport_v / static_cast<float>(image_height_);
        const auto viewport_ul = lookfrom_ - focal_len * w - 0.5f * (viewport_u + viewport_v);
        const auto pixel00 = viewport_ul + 0.5f * (pixel_du + pixel_dv);
        const auto pixel00_relative = pixel00 - lookfrom_;  // Precompute for device

        device_struct_.eye_ = utils::data::toFloat3(lookfrom_);
        device_struct_.pixel00_relative_ = utils::data::toFloat3(pixel00_relative);
        device_struct_.pixel_du_ = utils::data::toFloat3(pixel_du);
        device_struct_.pixel_dv_ = utils::data::toFloat3(pixel_dv);
    }
};

}  // namespace thesis::host::params
