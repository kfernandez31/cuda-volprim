#include "thesis/optix/launch_params.h"
#include "thesis/device/vector.h"

#include <optix.h>

#include "common.cuh"
#include "random.cuh"
#include "trace.cuh"

#define MAX_HITS 64

// TODO(kacper): maybe start without backface culling
// TODO(kacper): either shift the ray or modify min intersection distance

extern "C" __global__ void __raygen__rg() {
    using namespace thesis::device;

    auto acc_color = make_float3(0.0f);

    PriorityQueue<HitEvent, MAX_HITS> pq;
    Vector<Primitive*, MAX_HITS> active_prims;

    for (size_t sample = 0; sample < params.num_samples_per_pixel_; ++sample) {
        const auto jitter = sample_random_2d(s);
        const auto ray = compute_jittered_ray(jitter);

        float3 acc_optical_depth = make_float3(0.0f);
        float t_total = 0.0f;

        pq.clear();
        active_prims.clear();

        auto process_exited_prims = [&](float t_in) {
            while (!pq.empty() && pq.top().t_exit <= t_in) {
                const auto t_exit = pq.top().t_exit;
                pq.pop();

                const auto r = ray.advanced_by(t_total);
                const auto t_range = make_float2(0.0f, t_exit - t_total);

                for (const auto& prim : active_prims) { // TODO(kacper): is this in the right order?
                    acc_optical_depth += prim->density_integral(r, t_range);
                }

                t_total = t_exit;
            }
        };

        for (int hit = 0; hit < MAX_HITS; ++hit) {
            unsigned int t_raw, prim_idx, is_exit;
            trace(ray, 0.0f, t_raw, prim_idx, is_exit);

            float t_hit = __uint_as_float(t_raw);
            if (t_hit >= INF_F) {
                break;
            }

            auto t_range = make_float2(0.0f, t_hit);
            auto r = ray.advanced_by(t_total);

            process_exited_prims(t_total + t_hit);

            for (const auto& prim : active_prims) { // TODO(kacper): is this in the right order?
                acc_optical_depth += prim->density_integral(r, t_range);
            }

            // TODO(kacper): don't we want to break when our vector is full?
            if (!is_exit && !active_prims.full()) {
                active_prims.emplace_back(&params.primitives_[prim_idx]);
                pq.emplace({t_total + t_hit, prim_idx}); // TODO(kacper): verify correctness
            }

            ray.march_by(t_hit);
            t_total += t_hit;
        }

        process_exited_prims(INF_F);

        acc_color += expf(-acc_optical_depth) * background_color(r); // TODO(kacper): jak tu samplować
    }

    acc_color /= static_cast<float>(params.num_samples_per_pixel_);

    params.image_(idx.x, idx.y) = acc_color;
}
