# Problem: Efficient Volumetric Primitive Density Integral

You are given a scene `ObjectList world` containing raytracable primitives. When you shoot a ray and it hits a primitive along its path, we take note of the entry and exit point of the ray. Given those points, we can calculate the density integral (which you can think of as the accummulation of matter) between them, which quantifies how much light energy the ray loses when having to go through said primitive.

By the way, you can assume that `world::intersect` automatically performs *backface culling*, i.e. the returned hit record is that of the closest primitive which we hit from outside, not inside. This is a common optimization that allows us to not consider primitives already processed.

## Context

```cpp
class Interval {
public:
    float min, max;

    Interval() = default;

    Interval(T min, T max)
        : min(std::min(min, max)), max(std::max(min, max)) {}
};

class Ray {
public:
    vec3 origin, direction;

    Ray(const vec3& _origin, const vec3& _direction, bool normalize=true)
        : origin(_origin), direction(normalize ? glm::normalize(_direction) : _direction) {}

    inline vec3 at(float t) const {
        return origin + t * direction;
    }

    inline void march_by(float t, float offset=1e-8 /* small offset to avoid self-intersections */) {
        origin = at(t + offset);
    }
};


```cpp
vec3 ray_color(Ray r, Object& world, size_t max_depth) const {
    std::optional<HitRecord> hit;
    vec3 acc_optical_depth(0);
    for (size_t i = 0; i < max_depth; ++i, r.march_by(hit->t_in)) {
        auto hit = world.intersect(r);
        if (!hit)
            break;

        acc_optical_depth += hit->object->albedo * hit->object->optical_depth(r, {hit->t_in, hit->t_out});
    }

    auto final_transmittance = glm::exp(-acc_optical_depth); // exponential decay
    return final_transmittance * background_color(r);
}
```

## Current, naive implementation

```cpp
static std::vector<std::pair<Interval, std::vector<size_t>>> get_segment_to_primitive_mapping(const std::vector<Interval>& input) {
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
    std::vector<Interval> intervals;
    std::vector<std::shared_ptr<Object>> primitives;
    std::vector<Ray> rays;

    std::unordered_set<ObjectId> prims_hit;
    std::optional<HitRecord> hit;

    // 1. March the ray along the scene, collecting primitives along the way.
    for (size_t i = 0; i < max_depth; ++i, r.march_by(hit->t_in)) {
        hit = world.intersect(r);
        if (!hit)
            break;

        // collected a new primitive
        prims_hit.insert(hit->object->id);
        intervals.emplace_back(hit->t_in, hit->t_out);
        primitives.push_back(hit->object);
        rays.push_back(r);
    }

    // 2. Obtain the mapping and for each segment collect the contribution of its primitives.
    auto mapping = get_segment_to_primitive_mapping(intervals);
    vec3 acc_optical_depth(0);
    for (const auto& [interval, indices] : mapping) {
        for (auto idx : indices) {
            const auto& prim = primitives[idx];
            const auto& ray = rays[idx];
            acc_optical_depth += prim->albedo * prim->density_integral(ray, interval);
        }
    }

    // 3. Compute final transmittance
    auto final_transmittance = glm::exp(-acc_optical_depth); // exponential decay
    return final_transmittance * background_color(r);
}
```

The complexity is $O(min(max\_depth, k) + k\log k = O(k\log k)$ in practice, where $k$ is the number of primitives. I'm saying "in practice", since we can set an arbitrarily large $max\_depth$ and so you can omit this parameter in your considerations altogether.

## Improved approach

What we can do instead of 

```cpp
vec3 ray_color(Ray r, ObjectList& world, size_t max_depth) {
    struct PrimRecord {
        float entry, exit
    }

    std::unordered_set<std::shared_ptr<Object>> active_primitives;

    std::optional<HitRecord> hit;
    float step_size = 1e-3f;  // Small step for marching

    float t_out;

    vec3 acc_optical_depth(0.0f);

    for (size_t i = 0; i < max_depth; ++i) {
        // Check new intersections at the current ray position
        hit = world.intersect(r);
        if (!hit)
            break;

        active_primitives.insert(hit->object);

        // Remove primitives that end (exit condition)
        for (auto it = active_primitives.begin(); it != active_primitives.end(); ) {
            const auto& prim = *it;
            if (t_cur >= prim->exit_time)
                it = active_primitives.erase(it);
            else
                ++it;
        }

        // Compute density contribution dynamically
        if (!active_primitives.empty()) {
            for (auto idx : active_primitives) {
                const auto& prim = primitives[idx];
                acc_optical_depth += prim->albedo * prim->density_integral(r, {t_cur, t_next});
            }
        }
    }

    // Compute final transmittance
    auto final_transmittance = glm::exp(-acc_optical_depth); // exponential decay
    return final_transmittance * background_color(r);
}


```