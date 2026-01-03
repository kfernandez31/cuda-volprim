#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_functions.h>
#include <vector_types.h>

#include <cmath>

// Vector convenience overloads in global namespace (where float3/float4 live)
THESIS_HOST_DEVICE THESIS_INLINE float3 make_float3(float s) noexcept {
    return make_float3(s, s, s);
}

THESIS_HOST_DEVICE THESIS_INLINE float3 make_float3(float4 v) noexcept {
    return make_float3(v.x, v.y, v.z);
}

THESIS_HOST_DEVICE THESIS_INLINE float4 make_float4(float s) noexcept {
    return make_float4(s, s, s, s);
}

THESIS_HOST_DEVICE THESIS_INLINE float4 make_float4(float3 xyz) noexcept {
    return make_float4(xyz.x, xyz.y, xyz.z, 0.0f);
}

// Forward declaration for operators (defined in thesis::common::math below)
namespace thesis {
namespace common {
namespace math {
THESIS_HOST_DEVICE THESIS_INLINE float rcp(float x) noexcept;
THESIS_HOST_DEVICE THESIS_INLINE float3 rcp(float3 v) noexcept;
}  // namespace math
}  // namespace common
}  // namespace thesis

// Vector operators in global namespace
// float2 operators
THESIS_HOST_DEVICE THESIS_INLINE float2 operator+(float2 a, float s) noexcept {
    return make_float2(a.x + s, a.y + s);
}

THESIS_HOST_DEVICE THESIS_INLINE float2 operator-(float2 a, float s) noexcept {
    return a + (-s);
}

// float3 unary negation (must be defined before compound operators use it)
THESIS_HOST_DEVICE THESIS_INLINE float3 operator-(float3 a) noexcept {
    return make_float3(-a.x, -a.y, -a.z);
}

// float3 compound assignment operators (primitives)
THESIS_HOST_DEVICE THESIS_INLINE void operator+=(float3& a, float3 b) noexcept {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
}

THESIS_HOST_DEVICE THESIS_INLINE void operator-=(float3& a, float3 b) noexcept {
    a += (-b);  // Defined in terms of addition
}

THESIS_HOST_DEVICE THESIS_INLINE void operator*=(float3& a, float s) noexcept {
    a.x *= s;
    a.y *= s;
    a.z *= s;
}

THESIS_HOST_DEVICE THESIS_INLINE void operator*=(float3& a, float3 b) noexcept {
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
}

THESIS_HOST_DEVICE THESIS_INLINE void operator/=(float3& a, float s) noexcept {
    a *= thesis::common::math::rcp(s);  // Defined in terms of multiplication
}

// float3 binary operators (implemented in terms of compound assignment)
THESIS_HOST_DEVICE THESIS_INLINE float3 operator+(float3 a, float3 b) noexcept {
    a += b;
    return a;
}

THESIS_HOST_DEVICE THESIS_INLINE float3 operator-(float3 a, float3 b) noexcept {
    a += (-b);  // Defined in terms of addition
    return a;
}

THESIS_HOST_DEVICE THESIS_INLINE float3 operator*(float3 a, float s) noexcept {
    a *= s;
    return a;
}

THESIS_HOST_DEVICE THESIS_INLINE float3 operator*(float s, float3 a) noexcept {
    return a * s;
}

THESIS_HOST_DEVICE THESIS_INLINE float3 operator*(float3 a, float3 b) noexcept {
    a *= b;
    return a;
}

THESIS_HOST_DEVICE THESIS_INLINE float3 operator/(float3 a, float s) noexcept {
    a *= thesis::common::math::rcp(s);  // Defined in terms of multiplication
    return a;
}

THESIS_HOST_DEVICE THESIS_INLINE float3 operator/(float3 a, float3 b) noexcept {
    a *= thesis::common::math::rcp(b);  // Defined in terms of multiplication
    return a;
}

// float4 operators
THESIS_HOST_DEVICE THESIS_INLINE void operator+=(float4& a, float4 b) noexcept {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    a.w += b.w;
}

THESIS_HOST_DEVICE THESIS_INLINE float4 operator+(float4 a, float4 b) noexcept {
    a += b;
    return a;
}

THESIS_HOST_DEVICE THESIS_INLINE float4 operator*(float4 a, float s) noexcept {
    return make_float4(a.x * s, a.y * s, a.z * s, a.w * s);
}

THESIS_HOST_DEVICE THESIS_INLINE float4 operator/(float4 a, float s) noexcept {
    return a * thesis::common::math::rcp(s);
}

namespace thesis {
namespace common {
namespace math {

// Reciprocal with optional numerical guards
THESIS_HOST_DEVICE THESIS_INLINE float rcp(float x) noexcept {
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
    return (x != 0.0f) ? (1.0f / x) : 0.0f;
#else
    #ifdef DEVICE
        return __frcp_rn(x);  // Fast reciprocal (round-to-nearest)
    #else
        return 1.0f / x;
    #endif
#endif
}

THESIS_HOST_DEVICE THESIS_INLINE float3 rcp(float3 v) noexcept {
    return make_float3(rcp(v.x), rcp(v.y), rcp(v.z));
}

constexpr float PI_F = 3.14159265358979323846f;
constexpr float TWO_PI_F = 2.0f * PI_F;
constexpr float FOUR_PI_F = 4.0f * PI_F;
constexpr float ONE_OVER_PI_F = 1.0f / PI_F;
constexpr float ONE_OVER_TWO_PI_F = 1.0f / (2.0f * PI_F);
constexpr float ONE_OVER_FOUR_PI_F = 1.0f / (4.0f * PI_F);
constexpr float ONE_OVER_TWO_PI_POW_3_2_F = 0.0634936359f;
constexpr float ROOT_TWO_PI_F = 2.5066282746f;
constexpr float DEG_TO_RAD_F = PI_F / 180.0f;
constexpr float RAD_TO_DEG_F = 180.0f / PI_F;

constexpr float ROOT_TWO_F = 1.41421356237309504880f;
constexpr float TWO_ROOT_TWO_F = 2.0f * ROOT_TWO_F;
constexpr float FOUR_ROOT_TWO_F = 4.0f * ROOT_TWO_F;
constexpr float ONE_OVER_ROOT_TWO_F = 1.0f / ROOT_TWO_F;
constexpr float ONE_OVER_TWO_ROOT_TWO_F = 1.0f / (2.0f * ROOT_TWO_F);
constexpr float ONE_OVER_FOUR_ROOT_TWO_F = 1.0f / (4.0f * ROOT_TWO_F);

template <typename T, typename Exponent>
THESIS_HOST_DEVICE constexpr T pow(T base, Exponent exp) noexcept {
    return (exp == 0) ? T(1) : base * pow(base, exp - 1);
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float pow2(float a) noexcept {
    return a * a;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float3 pow2(float3 v) noexcept {
    return {pow2(v.x), pow2(v.y), pow2(v.z)};
}

template <typename T>
THESIS_HOST_DEVICE constexpr T min(T a) noexcept {
    return a;
}

template <typename T, typename... Ts>
THESIS_HOST_DEVICE constexpr T min(T a, Ts... args) noexcept {
    T m = min(args...);
    return a < m ? a : m;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float min(float3 v) noexcept {
    return min(min(v.x, v.y), v.z);
}

template <typename T>
THESIS_HOST_DEVICE constexpr T max(T a) noexcept {
    return a;
}

template <typename T, typename... Ts>
THESIS_HOST_DEVICE constexpr T max(T a, Ts... args) noexcept {
    T m = max(args...);
    return a > m ? a : m;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float max(float3 v) noexcept {
    return max(max(v.x, v.y), v.z);
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float clamp(float x, float lo, float hi) noexcept {
    return max(lo, min(x, hi));
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float sum(float3 v) noexcept {
    return v.x + v.y + v.z;
}

THESIS_HOST_DEVICE THESIS_INLINE constexpr float prod(float3 v) noexcept {
    return v.x * v.y * v.z;
}

// Scalar FMA (fmaf is standard C, compiles to hardware FMA on device)
THESIS_HOST_DEVICE THESIS_INLINE float fma(float a, float b, float c) noexcept {
    return fmaf(a, b, c);
}

// Standard C math wrappers (work on host and device)
THESIS_HOST_DEVICE THESIS_INLINE float sqrt(float x) noexcept {
#ifdef DEVICE
    return __fsqrt_rn(x);  // Fast sqrt (round-to-nearest)
#else
    return sqrtf(x);
#endif
}
THESIS_HOST_DEVICE THESIS_INLINE float exp(float x) noexcept {
    return expf(x);
}
THESIS_HOST_DEVICE THESIS_INLINE float3 exp(float3 v) noexcept {
    return make_float3(exp(v.x), exp(v.y), exp(v.z));
}
THESIS_HOST_DEVICE THESIS_INLINE float log(float x) noexcept {
    return logf(x);
}
THESIS_HOST_DEVICE THESIS_INLINE float sin(float x) noexcept {
    return sinf(x);
}
THESIS_HOST_DEVICE THESIS_INLINE float cos(float x) noexcept {
    return cosf(x);
}
THESIS_HOST_DEVICE THESIS_INLINE float abs(float x) noexcept {
    return fabsf(x);
}
THESIS_HOST_DEVICE THESIS_INLINE float copysign(float x, float y) noexcept {
    return copysignf(x, y);
}
THESIS_HOST_DEVICE THESIS_INLINE float erf(float x) noexcept {
    return erff(x);
}
THESIS_HOST_DEVICE THESIS_INLINE float erfc(float x) noexcept {
    return erfcf(x);
}
THESIS_HOST_DEVICE THESIS_INLINE float acos(float x) noexcept {
    return acosf(x);
}
THESIS_HOST_DEVICE THESIS_INLINE float atan2(float y, float x) noexcept {
    return atan2f(y, x);
}

// Reciprocal square root
THESIS_HOST_DEVICE THESIS_INLINE float rsqrt(float x) noexcept {
#ifdef DEVICE
    return rsqrtf(x);
#else
    // Quake III fast inverse square root with Newton-Raphson refinement
    const auto neg_xhalf = -0.5f * x;
    auto i = *reinterpret_cast<const int*>(&x);
    i = 0x5f3759df - (i >> 1);
    auto y = *reinterpret_cast<const float*>(&i);
    y *= fma(pow2(y), neg_xhalf, 1.5f);  // Newton-Raphson iteration
    return y;
#endif
}

THESIS_HOST_DEVICE THESIS_INLINE float dot(float3 a, float3 b) noexcept {
    return fma(a.x, b.x, fma(a.y, b.y, a.z * b.z));
}

THESIS_HOST_DEVICE THESIS_INLINE float length2(float3 v) noexcept {
    return math::dot(v, v);
}

THESIS_HOST_DEVICE THESIS_INLINE float length(float3 v) noexcept {
    return sqrt(length2(v));
}

THESIS_HOST_DEVICE THESIS_INLINE float rlength(float3 v) noexcept {
    return rsqrt(length2(v));
}

THESIS_HOST_DEVICE THESIS_INLINE float3 normalize(float3 v) noexcept {
    return v * rlength(v);
}

THESIS_HOST_DEVICE THESIS_INLINE float dot(float4 a, float4 b) noexcept {
    return fma(a.x, b.x, fma(a.y, b.y, fma(a.z, b.z, a.w * b.w)));
}

THESIS_HOST_DEVICE THESIS_INLINE float length2(float4 v) noexcept {
    return math::dot(v, v);
}

THESIS_HOST_DEVICE THESIS_INLINE float length(float4 v) noexcept {
    return sqrt(length2(v));
}

THESIS_HOST_DEVICE THESIS_INLINE float rlength(float4 v) noexcept {
    return rsqrt(length2(v));
}

// FMA-optimized cross product
// cross(a, b) = (a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x)
// Each component uses: fma(a, b, -(c*d)) = a*b - c*d
THESIS_HOST_DEVICE THESIS_INLINE float3 cross(float3 a, float3 b) noexcept {
    return make_float3(fma(a.y, b.z, -(a.z * b.y)), fma(a.z, b.x, -(a.x * b.z)),
                       fma(a.x, b.y, -(a.y * b.x)));
}

THESIS_HOST_DEVICE THESIS_INLINE float3 fma(float a, float3 b, float3 c) noexcept {
    return make_float3(fma(a, b.x, c.x), fma(a, b.y, c.y), fma(a, b.z, c.z));
}

// Linear interpolation: lerp(a, b, t) = a + t * (b - a) = (1-t)*a + t*b
THESIS_HOST_DEVICE THESIS_INLINE float lerp(float a, float b, float t) noexcept {
    return fma(t, b - a, a);
}

THESIS_HOST_DEVICE THESIS_INLINE float3 lerp(float3 a, float3 b, float t) noexcept {
    return fma(t, b - a, a);
}

// Midpoint: midpoint(a, b) = lerp(a, b, 0.5)
THESIS_HOST_DEVICE THESIS_INLINE float midpoint(float a, float b) noexcept {
    return lerp(a, b, 0.5f);
}

THESIS_HOST_DEVICE THESIS_INLINE float3 midpoint(float3 a, float3 b) noexcept {
    return lerp(a, b, 0.5f);
}

// Compute next power of 2 >= n using bit manipulation
template <typename UInt>
THESIS_HOST_DEVICE THESIS_INLINE constexpr UInt next_power_of_2(UInt n) noexcept {
    static_assert(std::is_unsigned<UInt>::value, "next_power_of_2 requires unsigned integer type");

    if (n == 0)
        return 1;
    --n;

    // Unroll for all possible bit widths using if constexpr
    constexpr size_t bits = sizeof(UInt) * 8;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    if constexpr (bits >= 16)
        n |= n >> 8;
    if constexpr (bits >= 32)
        n |= n >> 16;
    if constexpr (bits >= 64)
        n |= n >> 32;

    return n + 1;
}

// Sanitize: clamp to non-negative and filter NaN/Inf
// When THESIS_ENABLE_NUMERICAL_GUARDS is disabled, acts as identity
THESIS_HOST_DEVICE THESIS_INLINE float sanitize(float x) noexcept {
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
    x = max(x, 0.0f);               // Clamp negative
    return isfinite(x) ? x : 0.0f;  // Filter NaN/Inf
#else
    return x;  // Identity
#endif
}

THESIS_HOST_DEVICE THESIS_INLINE float3 sanitize(float3 v) noexcept {
#ifdef THESIS_ENABLE_NUMERICAL_GUARDS
    return make_float3(sanitize(v.x), sanitize(v.y), sanitize(v.z));
#else
    return v;  // Identity
#endif
}

template <typename T>
THESIS_HOST_DEVICE constexpr T ceil_div(T num, T den) noexcept {
    return (num + den - 1) / den;
}

}  // namespace math
}  // namespace common
}  // namespace thesis
