#pragma once

#include "aabb.h"

class Flat : public Object {
public:
    Flat(const vec3& Q, const vec3& u, const vec3& v)
        : Q(Q), u(u), v(v)
    {
        auto n = glm::cross(u, v);
        auto n_length_invsqrt = glm::inversesqrt(glm::length2(n));
        normal = n * n_length_invsqrt;
        w = normal * n_length_invsqrt;
        D = glm::dot(normal, Q);
        bbox = AABB({Q, Q + u + v}, {Q + u, Q + v});
    }

    std::optional<HitRecord> intersect(const Ray& r) override {
        auto denom = glm::dot(normal, r.direction);
        if (glm::epsilonEqual(denom, 0.0f, 1e-8f))
            return {};

        auto t = (D - glm::dot(normal, r.origin)) / denom;
        if (t < 0.0f)
            return {};

        // Determine if hit point lies within the planar shape using its plane coordinates.
        auto intersection = r.at(t);
        auto planar_hitpt_vector = intersection - Q;
        auto alpha = glm::dot(w, glm::cross(planar_hitpt_vector, v));
        auto beta  = glm::dot(w, glm::cross(u, planar_hitpt_vector));

        if (!contains(t, alpha, beta))
           return {};
        return HitRecord(shared_from_this(), t, t);
    }

protected:
    virtual bool contains(float t, float alpha, float beta) const = 0;
private:
    vec3 Q, u, v, w, normal;
    AABB bbox; // TODO: use
    float D;
};
