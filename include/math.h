#pragma once

#include <cmath>
#include <random>

namespace math {

template <typename T>
inline T inf() {
    return std::numeric_limits<T>::infinity();
}

template<typename T>
T random(T min, T max) {
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev()); // TODO: explore alternatives: Ziggurat, Ratio-Of-Uniforms, glm::linearRand
    std::uniform_real_distribution<T> distr(min, max);
    return distr(generator);
}

template<typename T>
inline T random() {
    return random<T>(0.0, 1.0);
}

} /* namespace math */
