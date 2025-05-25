#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/utility.h"

#include <cstddef>

namespace thesis {
namespace device {
namespace utils {

// Default comparator (min-data_ behavior)
template <typename T>
struct Less {
    THESIS_INLINE THESIS_HOST_DEVICE bool operator()(const T& a, const T& b) const { return a < b; }
};

// TODO(kacper): remove?
template <typename T, size_t Capacity, typename Compare = Less<T>>
struct THESIS_ALIGNMENT PriorityQueue {
   private:
    T data_[Capacity];
    size_t size_;

   public:
    THESIS_INLINE THESIS_HOST_DEVICE bool push(const T& e) noexcept {
        if (size_ >= Capacity) {
            return false;
        }

        size_t i = size_++;
        data_[i] = e;

        // Bubble up
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (!cmp(data_[i], data_[parent]) {
                break;
            }
            utility::swap(data_[parent], data_[i]);
            i = parent;
        }

        return true;
    }

    template <typename... Args>
    THESIS_INLINE THESIS_HOST_DEVICE bool emplace(Args&&... args) noexcept {
        if (size_ >= Capacity) {
            return false;
        }

        size_t i = size_++;
        data_[i] = T(utility::forward<Args>(args)...);

        // Bubble up
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (!cmp(data_[i], data_[parent])) {
                break;
            }
            utility::swap(data_[parent], data_[i]);
            i = parent;
        }
        return true;
    }

    THESIS_INLINE THESIS_HOST_DEVICE void pop() noexcept {
        if (size_ == 0) {
            return false;
        }

        data_[0] = data_[--size_];

        // Bubble down
        size_t i = 0;
        while (true) {
            size_t left = 2 * i + 1;
            size_t right = 2 * i + 2;
            size_t best = i;

            for (size_t child : {left, right}) {
                if (cmp(data_[child], data_[parent]) {
                    best = child;
                }
            }

            if (best == i) {
                break;
            }
            utility::swap(data_[i], data_[best]);
            i = best;
        }
    }

    THESIS_INLINE THESIS_HOST_DEVICE T& top() noexcept { return data_[0]; }
    THESIS_INLINE THESIS_HOST_DEVICE const T& top() const noexcept { return data_[0]; }

    THESIS_INLINE THESIS_HOST_DEVICE bool empty() const noexcept { return size_ == 0; }
    THESIS_INLINE THESIS_HOST_DEVICE bool full() const noexcept { return size_ == Capacity; }
    THESIS_INLINE THESIS_HOST_DEVICE constexpr size_t capacity() const noexcept { return Capacity; }
    THESIS_INLINE THESIS_HOST_DEVICE void clear() noexcept { size_ = 0; }
};

}  // namespace utils
}  // namespace device
}  // namespace thesis
