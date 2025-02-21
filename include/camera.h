#pragma once

#include "object.h"
#include "math.h"

#include <glm/common.hpp>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

static constexpr size_t NUM_CHANNELS = 3;

class Camera {
public:
    struct CameraSettings {
        float  aspect_ratio      = 1.0;             // Ratio of image width over height
        size_t image_width       = 100;             // Rendered image width in pixel count
        size_t image_height      = 100;             // Rendered image height in pixel count
        size_t samples_per_pixel = 10;              // Count of random samples for each pixel
        size_t max_depth         = 10;              // Maximum number of ray bounces into scene
        float vertical_fov       = 90;              // Vertical view angle (field of view)
        vec3 lookfrom            = vec3(0);         // Point camera is looking from
        vec3 lookat              = vec3(0, 0, -1);  // Point camera is looking at
        vec3 vup                 = vec3(0, 1, 0);   // Camera-relative "up" direction

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

    // TODO: consider taking Object by shared_ptr

    void render(Object& world, const std::string& filename) const {
        std::vector<float> images[NUM_CHANNELS] = {
            std::vector<float>(settings.image_width * settings.image_height), // R
            std::vector<float>(settings.image_width * settings.image_height), // G
            std::vector<float>(settings.image_width * settings.image_height), // B
        };

        for (size_t j = 0; j < settings.image_height; ++j) {
            std::clog << "\rScanlines remaining: " << (settings.image_height - j) << ' ' << std::flush;
            for (size_t i = 0; i < settings.image_width; ++i) {
                auto color = sample_rays(world, i, j);

                // Store in EXR order (bottom to top)
                size_t idx = (settings.image_height - 1 - j) * settings.image_width + i;
                images[0][idx] = color.x;  // R
                images[1][idx] = color.y;  // G
                images[2][idx] = color.z;  // B
            }
        }
        std::clog << "\rDone.                 \n";

        save_exr_image(images, settings.image_width, settings.image_height, filename);
    }

private:
    CameraSettings settings;
    vec3 center;
    vec3 pixel00_loc;
    vec3 pixel_du, pixel_dv;

    void save_exr_image(std::vector<float> images[NUM_CHANNELS], int width, int height, const std::string& filename) const {
        EXRImage image;
        InitEXRImage(&image);

        float* image_ptrs[NUM_CHANNELS] = { images[2].data(), images[1].data(), images[0].data() }; // Reverse order for EXR
        image.images = reinterpret_cast<unsigned char**>(image_ptrs);
        image.width = width;
        image.height = height;
        image.num_channels  = NUM_CHANNELS;

        EXRHeader header;
        InitEXRHeader(&header);

        header.num_channels = NUM_CHANNELS;
        EXRChannelInfo channels[NUM_CHANNELS]  = { {"B"}, {"G"}, {"R"} };
        header.channels = channels;

        std::array<int, NUM_CHANNELS> pixel_types;
        pixel_types.fill(TINYEXR_PIXELTYPE_FLOAT);
        header.pixel_types = header.requested_pixel_types = pixel_types.data();

        if (const char* err = nullptr; SaveEXRImageToFile(&image, &header, filename.c_str(), &err) != TINYEXR_SUCCESS) {
            fprintf(stderr, "Error saving EXR: %s\n", err);
            FreeEXRErrorMessage(err);
        }
    }

    vec3 sample_rays(Object& world, size_t i, size_t j) const {
        vec3 pixel_color(0);
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

    vec3 background_color(const Ray& r) const {
        return vec3(1);
        // float a = 0.5f * (r.direction.y + 1.0f);
        // return glm::mix(vec3(1), vec3(0.5, 0.7, 1.0), a);
    }

    vec3 ray_color(const Ray& r, Object& world, size_t max_depth) const {
        auto hit = world.intersect(r);
        if (!hit)
            return background_color(r);

        return hit->object->albedo;
    }

/*
    vec3 ray_color(Ray r, Object& world, size_t max_depth) const {
        static constexpr auto ray_origin_offset = 1e-6f;  // Lower bound to avoid self-intersection

        vec3 acc_optical_depth(0);
        for (size_t i = 0; i < max_depth; ++i) {
            auto hit = world.intersect(r);
            if (!hit)
                break;

            Interval t_range(hit->t_in, hit->t_out);
            acc_optical_depth += hit->object->albedo * hit->object->optical_depth(r, t_range);
            r.origin += ray_origin_offset * r.direction;
        }

        auto final_transmittance = glm::exp(-acc_optical_depth); // exponential decay
        return final_transmittance * background_color(r);
    }
*/
};
