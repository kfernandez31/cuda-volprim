#pragma once

#include "thesis/utils/preprocessor.h"
#include "thesis/utils/utility.h"

#include <cstddef>

namespace thesis {
namespace device {

template <typename T, size_t Capacity>
class Set {
    T data_[Capacity];
    size_t size_ = 0;

public:
    Set() = default;

    THESIS_INLINE THESIS_HOST_DEVICE size_t size() const noexcept { return size_; }
    THESIS_INLINE THESIS_HOST_DEVICE constexpr size_t capacity() const noexcept { return Capacity; }
    THESIS_INLINE THESIS_HOST_DEVICE bool empty() const noexcept { return size_ == 0; }
    THESIS_INLINE THESIS_HOST_DEVICE bool full() const noexcept { return size_ == Capacity; }

    THESIS_INLINE THESIS_HOST_DEVICE void clear() noexcept { size_ = 0; }

    THESIS_INLINE THESIS_HOST_DEVICE bool contains(const T& value) const noexcept {
        for (size_t i = 0; i < size_; ++i) {
            if (data_[i] == value) {
                return true;
            }
        }
        return false;
    }

    THESIS_INLINE THESIS_HOST_DEVICE bool insert(const T& value) noexcept {
        if (contains(value) || full()) {
            return false;
        }
        data_[size_++] = value;
        return true;
    }

    // Erase if present (compacts array)
    THESIS_INLINE THESIS_HOST_DEVICE bool erase(const T& value) noexcept {
        for (size_t i = 0; i < size_; ++i) {
            if (data_[i] == value) {
                data_[i] = utility::move(data_[--size_]);
                return true;
            }
        }
        return false;
    }

    THESIS_INLINE THESIS_HOST_DEVICE T* begin() noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE T* end() noexcept { return data_ + size_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T* begin() const noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE const T* end() const noexcept { return data_ + size_; }
};

}  // namespace device
}  // namespace thesis