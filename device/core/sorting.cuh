#pragma once

#include "hit_record.cuh"
#include "thesis/device/utils/vector.h"

namespace thesis {
namespace device {

// Simple bubble sort for small-to-medium sized arrays
// Sorts HitRecords by t_hit in ascending order
// O(n²) complexity, but simple and correct for GPU
// TODO(kacper): Replace with radix sort if profiling shows this is a bottleneck
template <size_t N>
__device__ void sort(utils::StaticVector<HitRecord, N>& vec) {
    const size_t n = vec.size();
    if (n <= 1) return;  // Already sorted

    // Bubble sort: repeatedly swap adjacent elements if out of order
    for (size_t i = 0; i < n - 1; i++) {
        for (size_t j = 0; j < n - i - 1; j++) {
            if (vec[j].t_hit > vec[j + 1].t_hit) {
                // Swap
                HitRecord temp = vec[j];
                vec[j] = vec[j + 1];
                vec[j + 1] = temp;
            }
        }
    }
}

} // namespace device
} // namespace thesis
