#pragma once

#include "flat.h"

class Triangle : public Flat {
private:
    glm::vec3 points[3];
public:
    Triangle(const vec3& P0, const vec3& P1, const vec3& P2)
        : Flat(P0, P1 - P0, P2 - P0), points{P0, P1, P2} {}

    bool contains(float t, float alpha, float beta) const override {
        return alpha >= 0.0f && beta >= 0.0f && (alpha + beta) <= 1.0f;
    }
};
