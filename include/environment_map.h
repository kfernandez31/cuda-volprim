#pragma once

#include "vec.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/gtx/compatibility.hpp>

class EnvironmentMap {
public:
    float* data = nullptr;
    int width, height, channels;

    EnvironmentMap(const std::string& filepath) {
        stbi_set_flip_vertically_on_load(true);
        if (!(data = stbi_loadf(filepath.c_str(), &width, &height, &channels, 0)))
            throw std::runtime_error("Failed to load environment map");
    }

    ~EnvironmentMap() {
        stbi_image_free(data);
    }

    vec3 sample(const vec3& dir) const {
        auto theta = glm::atan2(dir.z, dir.x);                  // Azimuth angle
        auto phi   = glm::acos(glm::clamp(dir.y, -1.0f, 1.0f)); // Elevation angle

        // Normalize angles to [0,1]
        auto u = (theta + glm::pi<float>()) * glm::one_over_two_pi<float>();
        auto v = phi * glm::one_over_pi<float>();

        // Map UVs to pixel coordinates
        auto x = static_cast<size_t>(u * width) % width;
        auto y = static_cast<size_t>(v * height) % height;

        auto idx = (y * width + x) * channels;

        return {data[idx], data[idx + 1], data[idx + 2]};
    }
};