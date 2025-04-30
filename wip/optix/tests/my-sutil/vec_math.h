#pragma once

#include <sutil/Preprocessor.h>

#include <cuda_runtime.h>
#include <vector_types.h>
#include <vector_functions.h>
#include <type_traits>

#if !defined(__CUDACC_RTC__)
#include <cmath>
#include <cstdlib>
#endif

#ifndef M_PIf
#define M_PIf       3.14159265358979323846f
#endif
#ifndef M_PI_2f
#define M_PI_2f     1.57079632679489661923f
#endif
#ifndef M_1_PIf
#define M_1_PIf     0.318309886183790671538f
#endif

using uint = unsigned int;
using longlong = long long;
using ulonglong = unsigned long long;

// Map <T,N> to T##N CUDA vector type
template<typename T, int N> struct VecType;
#define BIND(T, N) template<> struct VecType<T, N> { using type = T##N; }
BIND(float, 2); BIND(float, 3); BIND(float, 4);
BIND(int, 2); BIND(int, 3); BIND(int, 4);
BIND(uint, 2); BIND(uint, 3); BIND(uint, 4);
BIND(longlong, 2); BIND(longlong, 3); BIND(longlong, 4);
BIND(ulonglong, 2); BIND(ulonglong, 3); BIND(ulonglong, 4);
#undef BIND

// Generic component access
template<int I, typename V>
THESIS_INLINE THESIS_HOSTDEVICE decltype(auto) component(V&& v) {
    if constexpr (I == 0) return std::forward<V>(v).x;
    if constexpr (I == 1) return std::forward<V>(v).y;
    if constexpr (I == 2) return std::forward<V>(v).z;
    if constexpr (I == 3) return std::forward<V>(v).w;
}

// Compile-time loop assign helper
template<typename T, typename S, int N, typename F, int... I>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<T, N>::type
loop_assign(const S& src, F func, std::integer_sequence<int, I...>) {
    typename VecType<T, N>::type out{};
    (func.template operator()<I>(out, src), ...);
    return out;
}

// Constructor from scalar list
template<typename T, int N>
struct ScalarAssign {
    const T* values;
    int count;
    template<int I>
    THESIS_INLINE THESIS_HOSTDEVICE void operator()(typename VecType<T, N>::type& out, const void*) const {
        component<I>(out) = I < count ? values[I] : T(0);
    }
};

template<typename T, int N, typename... Args>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<T, N>::type make_vec(Args... args) {
    static_assert(sizeof...(Args) >= 1 && sizeof...(Args) <= 4, "1 to 4 scalars required");
    T temp[] = { static_cast<T>(args)... };
    return loop_assign<T, void, N>({}, ScalarAssign<T, N>{temp, int(sizeof...(Args))}, std::make_integer_sequence<int, N>{});
}

// Copy / Convert same length
template<typename T, typename S, int N>
struct CopyOp {
    template<int I>
    THESIS_INLINE THESIS_HOSTDEVICE void operator()(typename VecType<T, N>::type& out, const typename VecType<S, N>::type& v) const {
        component<I>(out) = static_cast<T>(component<I>(v));
    }
};

template<typename T, typename S, int N>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<T, N>::type make_vec(const typename VecType<S, N>::type& v) {
    return loop_assign<T, typename VecType<S, N>::type, N>(v, CopyOp<T, S, N>{}, std::make_integer_sequence<int, N>{});
}

// Downcast from <S,K> to <T,N> where K > N
template<typename T, typename S, int N, int K>
struct DowncastOp {
    template<int I>
    THESIS_INLINE THESIS_HOSTDEVICE void operator()(typename VecType<T, N>::type& out, const typename VecType<S, K>::type& v) const {
        component<I>(out) = static_cast<T>(component<I>(v));
    }
};

template<typename T, typename S, int N, int K>
THESIS_INLINE THESIS_HOSTDEVICE typename std::enable_if<(K > N), typename VecType<T, N>::type>::type
make_vec(const typename VecType<S, K>::type& v) {
    return loop_assign<T, typename VecType<S, K>::type, N>(v, DowncastOp<T, S, N, K>{}, std::make_integer_sequence<int, N>{});
}

// Upcast from <S,K> to <T,N> where K < N
template<typename T, typename S, int N, int K>
struct UpcastOp {
    template<int I>
    THESIS_INLINE THESIS_HOSTDEVICE void operator()(typename VecType<T, N>::type& out, const typename VecType<S, K>::type& v) const {
        if constexpr (I < K) component<I>(out) = static_cast<T>(component<I>(v));
        else component<I>(out) = T(0);
    }
};

template<typename T, typename S, int N, int K>
THESIS_INLINE THESIS_HOSTDEVICE typename std::enable_if<(K < N), typename VecType<T, N>::type>::type
make_vec(const typename VecType<S, K>::type& v) {
    return loop_assign<T, typename VecType<S, K>::type, N>(v, UpcastOp<T, S, N, K>{}, std::make_integer_sequence<int, N>{});
}

// Unary map for floatN
template<int N, typename F, int... I>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<float, N>::type
map_components(const typename VecType<float, N>::type& v, F func, std::integer_sequence<int, I...>) {
    typename VecType<float, N>::type out{};
    ((component<I>(out) = func(component<I>(v))), ...);
    return out;
}

template<int N, typename F>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<float, N>::type
map_components(const typename VecType<float, N>::type& v, F func) {
    return map_components<N>(v, func, std::make_integer_sequence<int, N>{});
}

// Standard math ops
struct floor_op  { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float x) const { return ::floorf(x); } };
struct expf_op   { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float x) const { return ::expf(x);  } };
struct abs_op    { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float x) const { return ::fabsf(x); } };
struct sign_op   { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float x) const { return (x > 0) - (x < 0); } };
struct log_op    { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float x) const { return ::logf(x); } };

template<int N> THESIS_INLINE THESIS_HOSTDEVICE auto floor (const typename VecType<float, N>::type& v) { return map_components<N>(v, floor_op{}); }
template<int N> THESIS_INLINE THESIS_HOSTDEVICE auto expf  (const typename VecType<float, N>::type& v) { return map_components<N>(v, expf_op{}); }
template<int N> THESIS_INLINE THESIS_HOSTDEVICE auto abs   (const typename VecType<float, N>::type& v) { return map_components<N>(v, abs_op{}); }
template<int N> THESIS_INLINE THESIS_HOSTDEVICE auto sign  (const typename VecType<float, N>::type& v) { return map_components<N>(v, sign_op{}); }
template<int N> THESIS_INLINE THESIS_HOSTDEVICE auto log   (const typename VecType<float, N>::type& v) { return map_components<N>(v, log_op{}); }

// Binary operator (vector-vector)
template<typename T, int N, typename Op, int... I>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<T, N>::type
binary_op(const typename VecType<T, N>::type& a, const typename VecType<T, N>::type& b, Op op, std::integer_sequence<int, I...>) {
    typename VecType<T, N>::type out{};
    ((component<I>(out) = op(component<I>(a), component<I>(b))), ...);
    return out;
}

template<typename T, int N, typename Op>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<T, N>::type
binary_op(const typename VecType<T, N>::type& a, const typename VecType<T, N>::type& b, Op op) {
    return binary_op<T, N>(a, b, op, std::make_integer_sequence<int, N>{});
}

// Binary operator (vector-scalar)
template<typename T, int N, typename Op, int... I>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<T, N>::type
binary_scalar_op(const typename VecType<T, N>::type& a, T s, Op op, std::integer_sequence<int, I...>) {
    typename VecType<T, N>::type out{};
    ((component<I>(out) = op(component<I>(a), s)), ...);
    return out;
}

template<typename T, int N, typename Op>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<T, N>::type
binary_scalar_op(const typename VecType<T, N>::type& a, T s, Op op) {
    return binary_scalar_op<T, N>(a, s, op, std::make_integer_sequence<int, N>{});
}

template<typename T, int N, typename Op>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<T, N>::type
binary_scalar_op(T s, const typename VecType<T, N>::type& a, Op op) {
    return binary_scalar_op<T, N>(a, s, [&](T x, T y) { return op(y, x); });
}

// Operator objects
struct plus_op  { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float a, float b) const { return a + b; } };
struct minus_op { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float a, float b) const { return a - b; } };
struct mul_op   { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float a, float b) const { return a * b; } };
struct div_op   { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float a, float b) const { return a / b; } };

// Operators for all sides
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator+(const VecType<T, N>::type& a, const VecType<T, N>::type& b) { return binary_op<T, N>(a, b, plus_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator-(const VecType<T, N>::type& a, const VecType<T, N>::type& b) { return binary_op<T, N>(a, b, minus_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator*(const VecType<T, N>::type& a, const VecType<T, N>::type& b) { return binary_op<T, N>(a, b, mul_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator/(const VecType<T, N>::type& a, const VecType<T, N>::type& b) { return binary_op<T, N>(a, b, div_op{}); }

template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator+(const VecType<T, N>::type& a, T s) { return binary_scalar_op<T, N>(a, s, plus_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator-(const VecType<T, N>::type& a, T s) { return binary_scalar_op<T, N>(a, s, minus_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator*(const VecType<T, N>::type& a, T s) { return binary_scalar_op<T, N>(a, s, mul_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator/(const VecType<T, N>::type& a, T s) { return binary_scalar_op<T, N>(a, s, div_op{}); }

template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator+(T s, const VecType<T, N>::type& a) { return binary_scalar_op<T, N>(s, a, plus_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator-(T s, const VecType<T, N>::type& a) { return binary_scalar_op<T, N>(s, a, minus_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator*(T s, const VecType<T, N>::type& a) { return binary_scalar_op<T, N>(s, a, mul_op{}); }
template<typename T, int N> THESIS_INLINE THESIS_HOSTDEVICE auto operator/(T s, const VecType<T, N>::type& a) { return binary_scalar_op<T, N>(s, a, div_op{}); }

// dot product
template<int N, int... I>
THESIS_INLINE THESIS_HOSTDEVICE float dot_impl(const typename VecType<float, N>::type& a, const typename VecType<float, N>::type& b, std::integer_sequence<int, I...>) {
    return ((component<I>(a) * component<I>(b)) + ...);
}
template<int N>
THESIS_INLINE THESIS_HOSTDEVICE float dot(const typename VecType<float, N>::type& a, const typename VecType<float, N>::type& b) {
    return dot_impl<N>(a, b, std::make_integer_sequence<int, N>{});
}

// cross (only float3)
THESIS_INLINE THESIS_HOSTDEVICE float3 cross(const float3& a, const float3& b) {
    return make_float3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

// length, length_squared, normalize
template<int N> THESIS_INLINE THESIS_HOSTDEVICE float length_squared(const typename VecType<float, N>::type& v) { return dot<N>(v, v); }
template<int N> THESIS_INLINE THESIS_HOSTDEVICE float length(const typename VecType<float, N>::type& v) { return sqrtf(length_squared<N>(v)); }
template<int N> THESIS_INLINE THESIS_HOSTDEVICE auto normalize(const typename VecType<float, N>::type& v) { return v * (1.0f / length<N>(v)); }

// reflect
template<int N>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<float, N>::type reflect(
    const typename VecType<float, N>::type& i,
    const typename VecType<float, N>::type& n) {
    return i - n * (2.0f * dot<N>(n, i));
}

// faceforward
template<int N>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<float, N>::type faceforward(
    const typename VecType<float, N>::type& n,
    const typename VecType<float, N>::type& i,
    const typename VecType<float, N>::type& nref) {
    return n * copysignf(1.0f, dot<N>(i, nref));
}

// min, max, clamp
template<typename T>
THESIS_INLINE THESIS_HOSTDEVICE T clamp_scalar(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

struct min_op   { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float a, float b) const { return fminf(a, b); } };
struct max_op   { THESIS_INLINE THESIS_HOSTDEVICE float operator()(float a, float b) const { return fmaxf(a, b); } };

template<int N>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<float, N>::type min(
    const typename VecType<float, N>::type& a,
    const typename VecType<float, N>::type& b) {
    return binary_op<float, N>(a, b, min_op{});
}

template<int N>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<float, N>::type max(
    const typename VecType<float, N>::type& a,
    const typename VecType<float, N>::type& b) {
    return binary_op<float, N>(a, b, max_op{});
}

template<int N>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<float, N>::type clamp(
    const typename VecType<float, N>::type& v,
    float a, float b) {
    return map_components<N>(v, [=](float x) { return clamp_scalar(x, a, b); });
}

template<int N>
THESIS_INLINE THESIS_HOSTDEVICE typename VecType<float, N>::type clamp(
    const typename VecType<float, N>::type& v,
    const typename VecType<float, N>::type& lo,
    const typename VecType<float, N>::type& hi) {
    typename VecType<float, N>::type out{};
    #pragma unroll
    for (int i = 0; i < N; ++i)
        component<i>(out) = clamp_scalar(component<i>(v), component<i>(lo), component<i>(hi));
    return out;
}

// any / all (boolean reductions)
template<typename T, int N, int... I>
THESIS_INLINE THESIS_HOSTDEVICE bool all_impl(const typename VecType<T, N>::type& v, std::integer_sequence<int, I...>) {
    return (... && static_cast<bool>(component<I>(v)));
}
template<typename T, int N>
THESIS_INLINE THESIS_HOSTDEVICE bool all(const typename VecType<T, N>::type& v) {
    return all_impl<T, N>(v, std::make_integer_sequence<int, N>{});
}

template<typename T, int N, int... I>
THESIS_INLINE THESIS_HOSTDEVICE bool any_impl(const typename VecType<T, N>::type& v, std::integer_sequence<int, I...>) {
    return (... || static_cast<bool>(component<I>(v)));
}
template<typename T, int N>
THESIS_INLINE THESIS_HOSTDEVICE bool any(const typename VecType<T, N>::type& v) {
    return any_impl<T, N>(v, std::make_integer_sequence<int, N>{});
}

// equality / inequality
template<typename T, int N, int... I>
THESIS_INLINE THESIS_HOSTDEVICE bool equal_impl(const typename VecType<T, N>::type& a, const typename VecType<T, N>::type& b, std::integer_sequence<int, I...>) {
    return (... && (component<I>(a) == component<I>(b)));
}
template<typename T, int N>
THESIS_INLINE THESIS_HOSTDEVICE bool operator==(const typename VecType<T, N>::type& a, const typename VecType<T, N>::type& b) {
    return equal_impl<T, N>(a, b, std::make_integer_sequence<int, N>{});
}
template<typename T, int N>
THESIS_INLINE THESIS_HOSTDEVICE bool operator!=(const typename VecType<T, N>::type& a, const typename VecType<T, N>::type& b) {
    return !(a == b);
}