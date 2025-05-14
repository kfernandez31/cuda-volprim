#pragma once

#include "thesis/utils/preprocessor.h"
#include "thesis/utils/utility.h"

#include <cstddef>

namespace thesis {
namespace device {

// Default comparator (min-heap behavior)
template <typename T>
struct Less {
    THESIS_INLINE THESIS_HOST_DEVICE bool operator()(const T& a, const T& b) const {
        return a < b;
    }
};

template <typename T, size_t Capacity, typename Compare = Less<T>>
struct PriorityQueue {
    HitEvent heap[Capacity];
    size_t size;

    THESIS_INLINE THESIS_HOST_DEVICE bool push(const HitEvent& e) noexcept {
        if (size >= Capacity) {
            return false;
        }
    
        size_t i = size++;
        heap[i] = e;

        // Bubble up
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (!cmp(heap[i], heap[parent]) {
                break;
            }
            utility::swap(heap[parent], heap[i]);
            i = parent;
        }

        return true;
    }

    template <typename... Args>
    THESIS_INLINE THESIS_HOST_DEVICE bool emplace(Args&&... args) noexcept {
        if (size >= Capacity) {
            return false;
        }

        size_t i = size++;
        heap[i] = T(utility::forward<Args>(args)...);

        // Bubble up
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (!cmp(heap[i], heap[parent])) {
                break;
            }
            utility::swap(heap[parent], heap[i]);
            i = parent;
        }
        return true;
    }

    THESIS_INLINE THESIS_HOST_DEVICE void pop() noexcept {
        if (size == 0) return false;

        heap[0] = heap[--size];

        // Bubble down
        size_t i = 0;
        while (true) {
            size_t left = 2 * i + 1;
            size_t right = 2 * i + 2;
            size_t best = i;

            for (size_t child : {left, right}) {
                if (cmp(heap[child], heap[parent]) {
                    best = child;
                }
            }
            
            if (best == i) {
                break;
            }
            utility::swap(heap[i], heap[best]);
            i = best;
        }
    }

    
    THESIS_INLINE THESIS_HOST_DEVICE T& top() noexcept { return heap[0]; }
    THESIS_INLINE THESIS_HOST_DEVICE const T& top() const noexcept { return heap[0]; }

    THESIS_INLINE THESIS_HOST_DEVICE bool empty() const noexcept { return size == 0; }
    THESIS_INLINE THESIS_HOST_DEVICE bool full() const noexcept { return size == Capacity; }
    THESIS_INLINE THESIS_HOST_DEVICE constexpr size_t capacity() const noexcept { return Capacity; }
    THESIS_INLINE THESIS_HOST_DEVICE void clear() noexcept { size = 0; }
};

}  // namespace device
}  // namespace thesis
