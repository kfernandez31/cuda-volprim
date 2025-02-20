#pragma once

#include "vec.h"

#include <iostream>

inline float linear_to_gamma(float x) {
    return std::pow(std::max(0.0f, x), 1.0f / 2.2f);
    // alternatively:
    // return glm::sqrt(glm::max(0.0f, x));
}

// Translates the [0,1] component values to the byte range [0,255].
void write_color(std::ostream& out, const vec3& in_col) {
    static const Interval intensity(0, 0.999);

    vec3 out_col = map_vec(in_col, [](auto x) {
        return int(256 * intensity.clamp(linear_to_gamma(x)));
    });
    out << out_col << '\n';
}
