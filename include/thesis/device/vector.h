#pragma once

#include "thesis/utils/preprocessor.h"
#include "thesis/utils/utility.h"

#include <cstddef>

namespace thesis {
namespace device {

// TODO(kacper): will I use this?
template <typename T, size_t Capacity>
class Vector {
    T data_[Capacity];
    size_t size_ = 0;

public:
    // TODO(kacper): move and copy ctors
    Vector() = default;

    THESIS_INLINE THESIS_HOST_DEVICE size_t size() const noexcept { return size_; }
    THESIS_INLINE THESIS_HOST_DEVICE constexpr size_t capacity() const noexcept { return Capacity; }
    THESIS_INLINE THESIS_HOST_DEVICE bool empty() const noexcept { return size_ == 0; }
    THESIS_INLINE THESIS_HOST_DEVICE bool full() const noexcept { return size_ == Capacity; }

    THESIS_INLINE THESIS_HOST_DEVICE T& operator[](size_t i) noexcept { return data_[i]; }
    THESIS_INLINE THESIS_HOST_DEVICE const T& operator[](size_t i) const noexcept { return data_[i]; }

    THESIS_INLINE THESIS_HOST_DEVICE void clear() noexcept { size_ = 0; }

    THESIS_INLINE THESIS_HOST_DEVICE bool push_back(const T& value) noexcept {
        if (full()) {
            return false;
        }
        data_[size_++] = value;
        return true;
    }

    THESIS_INLINE THESIS_HOST_DEVICE bool pop_back() noexcept {
        if (empty()) {
            return false;
        }
        --size_;
        return true;
    }

    template <typename... Args>
    THESIS_INLINE THESIS_HOST_DEVICE bool emplace_back(Args&&... args) noexcept {
        if (full()) {
            return false;
        }
        data_[size_++] = T(utility::forward<Args>(args)...);
        return true;
    }

    THESIS_INLINE THESIS_HOST_DEVICE T* begin() noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE T* end() noexcept { return data_ + size_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T* begin() const noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE const T* end() const noexcept { return data_ + size_; }
};

}  // namespace device
}  // namespace thesis
