#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/utility.h"

namespace thesis {
namespace device {
namespace utils {

template <typename T, typename E>
struct Result {
    bool is_ok_;
    union {
        T ok_value_;
        E err_value_;
    };

    __device__ Result()
        : is_ok_(false),
          err_value_() {}
    __device__ Result(const T& ok)
        : is_ok_(true),
          ok_value_(ok) {}
    __device__ Result(const E& err)
        : is_ok_(false),
          err_value_(err) {}

    Result(const Result&) = default;
    Result& operator=(const Result&) = default;

    template <typename... Args>
    __device__ void emplace_ok(Args&&... args) {
        is_ok_ = true;
        ok_value_ = T(utility::forward<Args>(args)...);
    }

    template <typename... Args>
    __device__ void emplace_err(Args&&... args) {
        is_ok_ = false;
        err_value_ = E(utility::forward<Args>(args)...);
    }

    __device__ bool is_ok() const { return is_ok_; }
    __device__ bool is_err() const { return !is_ok_; }

    __device__ const T& unwrap() const { return ok_value_; }
    __device__ T& unwrap() { return ok_value_; }

    __device__ const E& unwrap_err() const { return err_value_; }
    __device__ E& unwrap_err() { return err_value_; }

    __device__ operator bool() const { return is_ok_; }

    __device__ const T& operator*() const { return ok_value_; }
    __device__ T& operator*() { return ok_value_; }
};

#ifdef __CUDA_ARCH__
template <typename T, typename E, typename... Args>
__device__ Result<T, E> make_ok(Args&&... args) {
    return Result<T, E>(T(utility::forward<Args>(args)...));
}

template <typename T, typename E, typename... Args>
__device__ Result<T, E> make_err(Args&&... args) {
    return Result<T, E>(E(utility::forward<Args>(args)...));
}
#endif  // DEVICE

}  // namespace utils
}  // namespace device
}  // namespace thesis
