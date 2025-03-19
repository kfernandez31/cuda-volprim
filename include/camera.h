#pragma once

#include "environment_map.h"
#include "object_list.h"
#include "math.h"

#include <omp.h>

#include <glm/common.hpp>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <array>
#include <vector>
#include <unordered_set>

#pragma omp declare reduction(+: glm::vec3 : omp_out += omp_in) initializer(omp_priv = glm::vec3(0))

static constexpr size_t NUM_CHANNELS = 3;

class Camera {
public:
    struct CameraSettings {
        float  aspect_ratio      = 1.0;             // Ratio of image width over height
        size_t image_width       = 100;             // Rendered image width in pixel count
        size_t image_height      = 100;             // Rendered image height in pixel count
        size_t samples_per_pixel = 10;              // Count of random samples for each pixel
        size_t max_depth         = 10;              // Maximum number of ray bounces into scene
        float  vertical_fov      = 90;              // Vertical view angle (field of view)
        vec3   lookfrom          = vec3(0);         // Point camera is looking from
        vec3   lookat            = vec3(0, 0, -1);  // Point camera is looking at
        vec3   vup               = vec3(0, 1, 0);   // Camera-relative "up" direction

        CameraSettings() = default;
        CameraSettings& operator=(const CameraSettings&) = default;

        CameraSettings(const CameraSettings& other) {
            *this = other;
            image_height = glm::max(1ul, size_t(image_width / aspect_ratio));
        }
    };

    Camera(const CameraSettings& _settings, const std::string& env_map_path)
        : settings(_settings)
        , center(settings.lookfrom)
        , env_map(env_map_path)
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

    void render(ObjectList& world, const std::string& filename) const {
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
    EnvironmentMap env_map;

    void save_exr_image(std::vector<float> images[NUM_CHANNELS], int width, int height, const std::string& filename) const {
        EXRImage image;
        InitEXRImage(&image);

        float* image_ptrs[NUM_CHANNELS] = { images[2].data(), images[1].data(), images[0].data() }; // Reverse order for EXR
        image.images = reinterpret_cast<unsigned char**>(image_ptrs);
        image.width = width;
        image.height = height;
        image.num_channels = NUM_CHANNELS;

        EXRHeader header;
        InitEXRHeader(&header);

        header.num_channels = NUM_CHANNELS;
        EXRChannelInfo channels[NUM_CHANNELS] = {
            { "B", TINYEXR_PIXELTYPE_FLOAT, 1, 1, 0, {0} },
            { "G", TINYEXR_PIXELTYPE_FLOAT, 1, 1, 0, {0} },
            { "R", TINYEXR_PIXELTYPE_FLOAT, 1, 1, 0, {0} },
        };
        header.channels = channels;

        std::array<int, NUM_CHANNELS> pixel_types;
        pixel_types.fill(TINYEXR_PIXELTYPE_FLOAT);
        header.pixel_types = header.requested_pixel_types = pixel_types.data();

        if (const char* err = nullptr; SaveEXRImageToFile(&image, &header, filename.c_str(), &err) != TINYEXR_SUCCESS) {
            fprintf(stderr, "Error saving EXR: %s\n", err);
            FreeEXRErrorMessage(err);
        }
    }

    vec3 sample_rays(ObjectList& world, size_t i, size_t j) const {
        vec3 pixel_color(0);
        // #pragma omp parallel for reduction(+:pixel_color)
        for (size_t _ = 0; _ < settings.samples_per_pixel; ++_) {
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

    inline vec3 background_color(const Ray& r) const {
        return env_map.sample(r.direction);
    }

    struct Event {
        float t;
        size_t index;
        bool pos;

        inline bool operator<(const Event& other) const {
            return t < other.t || (t == other.t && pos < other.pos);
        }
    };

    static std::vector<std::pair<Interval, std::vector<size_t>>> get_interval_overlaps(const std::vector<Interval>& input) {
        const auto N = input.size();
        if (N == 0)
            return {};

        std::vector<Event> events;
        events.reserve(2 * N);

        // Step 0: Split intervals into events
        for (size_t i = 0; i < N; ++i)
            events.insert(events.end(), {{input[i].min, i, 0}, {input[i].max, i, 1}});

        // Step 1: Sort events
        std::sort(events.begin(), events.end());

        std::vector<std::pair<Interval, std::vector<size_t>>> result;
        result.reserve(2 * N);

        std::unordered_set<size_t> active;

        auto t_prev = events.front().t;

        // Step 2: Sweep through events
        for (size_t i = 0; i < 2 * N; ++i) {
            auto t_cur = events[i].t;

            if (!active.empty() && t_prev != t_cur)
                result.push_back({{t_prev, t_cur}, std::vector<size_t>(active.begin(), active.end())});

            if (events[i].pos == 0)
                active.insert(events[i].index);
            else
                active.erase(events[i].index);

            t_prev = t_cur;
        }

        return result;
    }

    vec3 ray_color(Ray r, ObjectList& world, size_t max_depth) const {
        size_t num_primitives = Object::nextId.load();
        std::vector<Interval> intervals;
        std::vector<std::shared_ptr<Object>> primitives;
        std::vector<Ray> rays;

        intervals.reserve(num_primitives);
        primitives.reserve(num_primitives);
        rays.reserve(num_primitives);

        std::unordered_set<ObjectId> prims_hit;
        std::optional<HitRecord> hit;
        float t_min = 0.0f;
        // March the ray along the scene, collecting primitives along the way
        for (size_t i = 0; i < max_depth; ++i, r.march_by(hit->t_in)) {
            hit = world.intersect(r, t_min, prims_hit);
            if (!hit)
                break;

            // collected a new primitive
            prims_hit.insert(hit->object->id);
            intervals.emplace_back(hit->t_in, hit->t_out);
            primitives.push_back(hit->object);
            rays.push_back(r); // this may be suboptimal
        }

        auto overlaps = get_interval_overlaps(intervals);
        vec3 acc_optical_depth(0);
        for (const auto& [interval, indices] : overlaps) {
            for (auto idx : indices) {
                const auto& prim = primitives[idx];
                const auto& ray = rays[idx];
                acc_optical_depth += prim->albedo * prim->density_integral(ray, interval);
            }
        }

        auto final_transmittance = glm::exp(-acc_optical_depth); // exponential decay
        return final_transmittance * background_color(r);
    }
};
