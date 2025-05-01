#pragma once

#ifndef __CUDACC__

#include "thesis/device/camera.h"
#include "thesis/vec.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#include <cstddef>

namespace thesis {
namespace host {

struct Camera {
    // Camera configuration
    float aspect_ratio = 1.0f;
    size_t image_width = 100;
    size_t image_height = 100;
    float vertical_fov = 90.0f;

    glm::vec3 lookfrom = glm::vec3(0.0f);
    glm::vec3 lookat   = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 vup      = glm::vec3(0.0f, 1.0f, 0.0f);

    // Derived state
    glm::vec3 eye     = {};
    glm::vec3 pixel00 = {};
    glm::vec3 pixel_du = {};
    glm::vec3 pixel_dv = {};

    Camera() = default;

    void build() {
        image_height = std::max<size_t>(1, static_cast<size_t>(image_width / aspect_ratio));

        const auto theta = glm::radians(vertical_fov);
        const auto h = tanf(0.5f * theta);

        const auto w = glm::normalize(lookfrom - lookat);
        const auto u = glm::normalize(glm::cross(vup, w));
        const auto v = glm::cross(w, u);

        const auto focal_len = glm::length(lookfrom - lookat);
        const auto viewport_height = 2.0f * h * focal_len;
        const auto viewport_width = viewport_height * aspect_ratio;

        const auto viewport_u = u * viewport_width;
        const auto viewport_v = -v * viewport_height;

        pixel_du = viewport_u / static_cast<float>(image_width);
        pixel_dv = viewport_v / static_cast<float>(image_height);

        eye = lookfrom;
        const auto viewport_ul = eye - focal_len * w - 0.5f * (viewport_u + viewport_v);
        pixel00 = viewport_ul + 0.5f * (pixel_du + pixel_dv);
    }

    [[nodiscard]] device::Camera toDevice() const noexcept {
        return device::Camera{
            .eye     = to_float3(eye),
            .pixel00 = to_float3(pixel00),
            .du      = to_float3(pixel_du),
            .dv      = to_float3(pixel_dv)
        };
    }
};

} // namespace host
} // namespace thesis

#endif // __CUDACC__