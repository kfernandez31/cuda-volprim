#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/utility.h"

#include <cstddef>

namespace thesis {
namespace device {
namespace utils {

template <typename T, size_t Capacity>
class THESIS_ALIGNMENT BinarySet {
   private:
    T data_[Capacity];
    size_t size_ = 0;

    __device__ int lower_bound(const T& value) const noexcept {
        int lo = -1;
        int hi = static_cast<int>(size_);

        while (hi - lo > 1) {
            int mid = (lo + hi) / 2;
            if (data_[mid] < value) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        return hi;
    }

    THESIS_INLINE THESIS_HOST_DEVICE bool contains_at_idx(int idx, const T& value) const noexcept {
        return idx < static_cast<int>(size_) && data_[idx] == value;
    }

    THESIS_INLINE THESIS_HOST_DEVICE int binary_search(const T& value) const noexcept {
        int idx = lower_bound(value);
        return contains_at_idx(idx, value) ? idx : -1;
    }

   public:
    THESIS_INLINE THESIS_HOST_DEVICE size_t size() const noexcept { return size_; }
    THESIS_INLINE THESIS_HOST_DEVICE constexpr size_t capacity() const noexcept { return Capacity; }
    THESIS_INLINE THESIS_HOST_DEVICE bool empty() const noexcept { return size_ == 0; }
    THESIS_INLINE THESIS_HOST_DEVICE bool full() const noexcept { return size_ == Capacity; }

    THESIS_INLINE THESIS_HOST_DEVICE void clear() noexcept { size_ = 0; }

    THESIS_INLINE THESIS_HOST_DEVICE bool contains(const T& value) const noexcept {
        return binary_search(value) != -1;
    }

    THESIS_INLINE THESIS_HOST_DEVICE bool insert(const T& value) noexcept {
        if (full()) {
            return false;
        }

        int idx = lower_bound(value);
        if (contains_at_idx(idx, value)) {
            return false;
        }

        // Shift right to make room
        for (int i = static_cast<int>(size_); i > idx; --i) {
            data_[i] = data_[i - 1];
        }
        data_[idx] = value;
        ++size_;
        return true;
    }

    THESIS_INLINE THESIS_HOST_DEVICE bool erase(const T& value) noexcept {
        int idx = binary_search(value);
        if (idx == -1) {
            return false;
        }

        --size_;
        // Shift left to take room
        for (int i = idx; i < static_cast<int>(size_); ++i) {
            data_[i] = data_[i + 1];
        }
        return true;
    }

    THESIS_INLINE THESIS_HOST_DEVICE T* begin() noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE T* end() noexcept { return data_ + size_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T* begin() const noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE const T* end() const noexcept { return data_ + size_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T* cbegin() const noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE const T* cend() const noexcept { return data_ + size_; }
};

}  // namespace utils
}  // namespace device
}  // namespace thesis
