#pragma once

#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/type_trait.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <type_traits>

std::ostream& operator<<(std::ostream& os, const glm::vec3& vec) {
    os << vec.x << ' ' << vec.y << ' ' << vec.z;
    return os;
}

template <typename VecType, typename Func>
inline VecType map_vec_indices(Func f) {
    static_assert(glm::type<VecType>::is_vec, "VecType must be a glm::vec type");
    VecType result;
    for (glm::length_t i = 0; i < result.length(); ++i)
        result[i] = f(i);
    return result;
}

template <typename VecType>
inline VecType random_vec(float min, float max) {
    return map_vec_indices<VecType>([=](auto) { return math::random(min, max); });
}

template <typename VecType, typename Func>
inline VecType map_vec(const VecType& v, Func f) {
    return map_vec_indices<VecType>([&](auto i) { return f(v[i]); });
}

inline glm::vec3 get_diagonal(const glm::mat4& M) {
    return {M[0][0], M[1][1], M[2][2]};
}

inline void set_diagonal(glm::mat4& M, const glm::vec3& v) {
    for (glm::length_t i = 0; i < 3; ++i)
        M[i][i] = v[i];
}

// see: https://stackoverflow.com/questions/1046714/what-is-a-good-random-number-generator-for-a-game
inline glm::vec3 random_unit_vector() {
    /* ------ Approach 1 - spherical coordinates ------ */
    // float theta = glm::linearRand(0.0f, glm::two_pi<float>()); // Random angle in [0, 2π)
    // float phi = glm::acos(glm::linearRand(-1.0f, 1.0f));       // Random angle in [0, π]

    // float x = glm::sin(phi) * glm::cos(theta);
    // float y = glm::sin(phi) * glm::sin(theta);
    // float z = glm::cos(phi);

    // return glm::vec3(x, y, z);

    /* ------ Approach 2 - glm, see: https://stackoverflow.com/questions/71161952/how-random-is-glmsphericalrand-some-angles-seem-more-common-than-others ------ */
    return glm::sphericalRand(1.0f);

    /* ------ Approach 3 - Gaussian sampling ------ */
    // return glm::normalize(random_vec3());

    /* ------ Approach 4 - Marsaglia method, see: https://stackoverflow.com/questions/7280184/fast-uniformly-distributed-random-points-on-the-surface-of-a-unit-hemisphere ------ */
    // Interval thresh(1e-160, 1.0);
    // glm::vec3 v;
    // float lensq;
    // do {
    //     v = random_vec(-1.0, 1.0);
    //     lensq = glm::length2(v);
    // } while (!thresh.contains(lensq));
    // return v * glm::inversesqrt(lensq);
}

inline glm::vec3 random_on_hemisphere(const glm::vec3& normal) {
    auto v = random_unit_vector();
    return glm::faceforward(v, normal, -v);
}

inline bool near_zero(const glm::vec3& v, float eps = 1e-8) {
    return glm::all(glm::epsilonEqual(v, glm::vec3(0), eps));
    // alternatively:
    // return glm::length2(v) < eps * eps;
}

inline bool faces_front(const glm::vec3& I, const glm::vec3& N) {
    return glm::dot(I, N) > 0.0f;
}
