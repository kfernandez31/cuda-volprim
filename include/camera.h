#pragma once

#include "environment_map.h"
#include "exr.h"
#include "object_list.h"
#include "math.h"

#include <omp.h>

#include <glm/common.hpp>

#include <array>
#include <vector>
#include <unordered_set>

#pragma omp declare reduction(+: glm::vec3 : omp_out += omp_in) initializer(omp_priv = glm::vec3(0))

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
        std::vector<vec3> framebuffer;

        for (size_t j = 0; j < settings.image_height; ++j) {
            std::clog << "\rScanlines remaining: " << (settings.image_height - j) << ' ' << std::flush;
            for (size_t i = 0; i < settings.image_width; ++i) {
                auto color = sample_rays(world, i, j);
                auto idx = j * width + i;
                framebuffer[idx] = color;
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

    /* TODO: opt for the version below when porting the code to GPU
    vec3 ray_color(const Ray& ray, ObjectList& world, size_t max_depth) {
        struct ExitEvent { float t_exit; size_t prim_idx; };
        auto cmp = [](const ExitEvent& a, const ExitEvent& b) {
            return a.t_exit > b.t_exit; // Min-heap by t_exit
        };

        std::priority_queue<ExitEvent, std::vector<ExitEvent>, decltype(cmp)> pq(cmp); // TODO: replace with std::set to be able to iterate it
        std::vector<std::shared_ptr<Object>> primitives; // TODO: reserve min(max_depth, #primitives) space

        vec3 acc_optical_depth(0.0f);
        float t_total = 0.0f;
        Ray ray_cur(ray.origin, ray.direction);

        auto process_exited_prims = [&](float t_in) {
            while (!pq.empty()) {
                const auto& [t_exit, _] = pq.top();
                if (t_exit > t_in) break;

                auto r = ray.advanced_by(t_total);
                Interval i(0, t_exit - t_total);

                // Integrate active primitives (including the one we exit)
                for (const auto& [_, prim_idx] : pq) {
                    const auto& prim = primitives[prim_idx];
                    acc_optical_depth += prim->albedo * prim->density_integral(r, i);
                }

                pq.pop();
                t_total = t_exit;
            }
        };

        for (size_t _ = 0; _ < max_depth; ++_) {
            auto hit = world.intersect(ray_cur);
            if (!hit) break; // No more intersections

            auto t_in  = hit->t_in;
            auto t_out = hit->t_out;

            auto t_total_prev = t_total;
            process_exited_prims(t_total + t_in);

            auto r = ray.advanced_by(t_total);
            Interval i(0, t_in - (t_total - t_total_prev));

            // Integrate active primitives
            for (const auto& [_, prim_idx] : pq) {
                const auto& prim = primitives[prim_idx];
                acc_optical_depth += prim->albedo * prim->density_integral(r, i);
            }

            auto prim_idx = primitives.size();
            primitives.emplace_back(std::move(hit->prim));

            pq.emplace(t_total + t_out, prim_idx);
            ray_cur.march_by(t_in);
            t_total += t_in;
        }

        // Drain remaining exits
        process_exited_prims(std::numeric_limits<float>::infinity());

        return glm::exp(-acc_optical_depth) * background_color(r);
    }
    */
};
