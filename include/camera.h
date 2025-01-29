#pragma once

#include "object.h"
#include "color.h"
#include "math.h"

#include <glm/common.hpp>

class Camera {
public:
    struct CameraSettings {
        float  aspect_ratio      = 1.0;  // Ratio of image width over height
        size_t image_width       = 100;  // Rendered image width in pixel count
        size_t image_height      = 100;  // Rendered image height in pixel count
        size_t samples_per_pixel = 10;   // Count of random samples for each pixel
        size_t max_depth         = 10;   // Maximum number of ray bounces into scene
        float vertical_fov       = 90;   // Vertical view angle (field of view)
        glm::vec3 lookfrom       = glm::vec3(0);         // Point camera is looking from
        glm::vec3 lookat         = glm::vec3(0, 0, -1);  // Point camera is looking at
        glm::vec3 vup            = glm::vec3(0, 1, 0);   // Camera-relative "up" direction

        CameraSettings() = default;
        CameraSettings& operator=(const CameraSettings&) = default;

        CameraSettings(const CameraSettings& other) {
            *this = other;
            image_height = glm::max(1ul, size_t(image_width / aspect_ratio));
        }
    };

    Camera(const CameraSettings& _settings)
        : settings(_settings)
        , center(settings.lookfrom)
    {
        // Determine viewport dimensions.
        const auto focal_length = glm::length(settings.lookfrom - settings.lookat);
        const auto theta = glm::radians(settings.vertical_fov);
        const auto h = glm::tan(0.5f * theta);
        const auto viewport_height = 2.0f * h * focal_length;
        const auto viewport_width = viewport_height * (float(settings.image_width) / settings.image_height);

        const auto w = glm::normalize(settings.lookfrom - settings.lookat);
        const auto u = glm::normalize(glm::cross(settings.vup, w));
        const auto v = glm::cross(w, u);

        // Calculate the vectors across the horizontal and down the vertical viewport edges.
        const auto viewport_u =  u * viewport_width;    // Vector across viewport horizontal edge
        const auto viewport_v = -v * viewport_height;   // Vector down viewport vertical edge

        // Calculate the horizontal and vertical delta vectors from pixel to pixel.
        pixel_du = viewport_u / float(settings.image_width);
        pixel_dv = viewport_v / float(settings.image_height);

        // Calculate the location of the upper left pixel.
        const auto viewport_upper_left = center - (w * focal_length) - 0.5f * (viewport_u + viewport_v);
        pixel00_loc = viewport_upper_left + 0.5f * (pixel_du + pixel_dv);
    }

    void render(Object& world) const {
        std::cout << "P3\n" << settings.image_width << ' ' << settings.image_height << "\n255\n";

        for (size_t j = 0; j < settings.image_height; ++j) {
            std::clog << "\rScanlines remaining: " << (settings.image_height - j) << ' ' << std::flush;
            for (size_t i = 0; i < settings.image_width; ++i) {
                auto color = sample_rays(world, i, j);
                write_color(std::cout, color);
            }
        }
        std::clog << "\rDone.                 \n";
    }
private:
    CameraSettings settings;
    glm::vec3 center;
    glm::vec3 pixel00_loc;
    glm::vec3 pixel_du, pixel_dv;

    glm::vec3 sample_rays(Object& world, size_t i, size_t j) const {
        glm::vec3 pixel_color(0);
        for (size_t sample = 0; sample < settings.samples_per_pixel; ++sample) {
            auto r = get_ray(i, j);
            auto c = ray_color(r, world, settings.max_depth);
            pixel_color += c;
        }
        return pixel_color / float(settings.samples_per_pixel);
    }

    // Construct a camera ray originating from the origin and directed at randomly sampled point around the pixel location i, j.
    Ray get_ray(size_t i, size_t j) const {
        static const float range = 0.5;
        auto offset = random_vec<glm::vec2>(-range, range);
        auto pixel_sample = pixel00_loc + ((i + offset.x) * pixel_du) + ((j + offset.y) * pixel_dv);
        return Ray(center, pixel_sample - center);
    }

    glm::vec3 background_color(const Ray& r) const {
        float a = 0.5f * (r.direction.y + 1.0f);
        return glm::mix(glm::vec3(1), glm::vec3(0.5, 0.7, 1.0), a);
    }

    // TODO: make iterative
    glm::vec3 ray_color(const Ray& r, Object& world, size_t depth) const {
        if (depth == 0)
            return glm::vec3(0);

        static constexpr float eps = 1e-8; // lower bound to avoid self-intersection
        auto hit = world.intersect(r, {eps, math::inf<float>()});
        if (!hit)
            return background_color(r);

        Interval t_range{hit->t_in, hit->t_out};
        auto trans = hit->object->transmittance(r, t_range);
        return (1.0f - trans) * hit->object->color;
        // TODO: recursive call
    }

    // glm::vec3 ray_color(const Ray& r, const Object& world, size_t depth) const {
    //     if (depth == 0)
    //         return glm::vec3(0.0);

    //     auto hit = world.intersect(r, {0.001, math::inf<float>()});
    //     if (!hit.empty()) {
    //         const auto& entry_hit = hit[0]; // Entry point of intersection.
    //         const auto& exit_hit = hit[1];  // Exit point of intersection (assuming two hit for a bounded volume).

    //         // Compute transmittance along the ray segment inside the object.
    //         float T = world.transmittance(r, {entry_hit->t, exit_hit->t});

    //         // Compute the resulting color:
    //         // - Transmitted background color
    //         glm::vec3 background_color = glm::mix(glm::vec3(1.0), glm::vec3(0.5, 0.7, 1.0), 0.5f * (r.direction.y + 1.0f));

    //         // - Scattered/absorbed ellipsoid contribution
    //         glm::vec3 ellipsoid_color = 0.5f * (entry_hit->object->material.color + 1.0f);

    //         // Final blended color using transmittance
    //         glm::vec3 C_result = T * background_color + (1.0f - T) * ellipsoid_color;

    //         // Recursive reflection/scattering for indirect illumination
    //         glm::vec3 scattered_dir = random_on_hemisphere(entry_hit->normal);
    //         // glm::vec3 indirect_color = ray_color(Ray(entry_hit->p, scattered_dir), world, depth - 1);
    //         return C_result;

    //         // Blend direct and indirect contributions
    //         // return C_result * 0.5f + indirect_color * 0.5f;
    //     }

    //     // Background color if no intersection
    //     float a = 0.5f * (r.direction.y + 1.0f);
    //     return glm::mix(glm::vec3(1.0), glm::vec3(0.5, 0.7, 1.0), a);
    // }

};
