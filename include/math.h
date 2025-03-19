#pragma once

#include <cmath>
#include <random>

namespace math {

template <typename T>
inline T inf() {
    return std::numeric_limits<T>::infinity();
}

// TODO: explore alternatives to mt19937: Ziggurat, Ratio-Of-Uniforms, glm::linearRand, ...

template<typename T>
T random_real(T min, T max) {
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_real_distribution<T> distr(min, max);
    return distr(generator);
}

template<typename T>
T random_int(T min, T max) {
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_int_distribution<T> distr(min, max);
    return distr(generator);
}

template<typename T>
inline T random() {
    return random<T>(0.0, 1.0);
}

} /* namespace math */
