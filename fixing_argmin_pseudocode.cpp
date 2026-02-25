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
