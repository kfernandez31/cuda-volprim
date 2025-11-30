#pragma once

#include "hit_record.cuh"
#include "thesis/device/utils/vector.h"

namespace thesis {
namespace device {

// ============================================================================
// Warp Shuffle Sort (for n <= 32)
// ============================================================================
// Uses warp shuffle intrinsics for ultra-fast sorting of small arrays
// Bitonic sort with warp shuffles - O(log²n) with minimal divergence
template <size_t N>
__device__ void warp_shuffle_sort(utils::StaticVector<HitRecord, N>& vec) {
    const auto n = vec.size();

    const auto lane_id = static_cast<size_t>(threadIdx.x & 31);

    // Each lane holds one element (pad with sentinel for unused lanes)
    HitRecord my_record = (lane_id < n) ? vec[lane_id] : HitRecord{consts::INF_F, 0u, false};

    // Bitonic sort with warp shuffles
    #pragma unroll
    for (int k = 2; k <= 32; k *= 2) {
        #pragma unroll
        for (int j = k / 2; j > 0; j /= 2) {
            const int ixj = lane_id ^ j;
            const HitRecord other = HitRecord{
                __shfl_sync(0xFFFFFFFF, my_record.t_hit, ixj),
                __shfl_sync(0xFFFFFFFF, my_record.prim_idx, ixj),
                __shfl_sync(0xFFFFFFFF, my_record.is_exit, ixj)
            };

            const bool ascending = ((lane_id & k) == 0);
            const bool should_swap = (ascending ? (my_record.t_hit > other.t_hit)
                                                : (my_record.t_hit < other.t_hit));

            if (ixj > lane_id && should_swap) {
                my_record = other;
            }
        }
    }

    // Write back sorted results
    if (lane_id < n) {
        vec[lane_id] = my_record;
    }
}

// ============================================================================
// Insertion Sort (for 32 < n <= 128)
// ============================================================================
// O(n²) but with better constants than bubble sort, good cache behavior
template <size_t N>
__device__ __forceinline__ void insertion_sort(utils::StaticVector<HitRecord, N>& vec) {
    const auto n = vec.size();

    for (size_t i = 1; i < n; ++i) {
        HitRecord key = vec[i];
        auto j = static_cast<int>(i) - 1;

        // Shift elements greater than key to the right
        while (j >= 0 && vec[j].t_hit > key.t_hit) {
            vec[j + 1] = vec[j];
            j--;
        }
        vec[j + 1] = key;
    }
}

// ============================================================================
// Bitonic Sort (for n > 128)
// ============================================================================
// O(n log²n) parallel sorting network
template <size_t N>
__device__ void bitonic_sort(utils::StaticVector<HitRecord, N>& vec) {
    const auto n = vec.size();

    // Bitonic sort requires power-of-2 size, so we pad conceptually
    // by treating out-of-bounds as infinity
    auto get_val = [&](size_t idx) -> float {
        return (idx < n) ? vec[idx].t_hit : consts::INF_F;
    };

    auto compare_and_swap = [&](size_t i, size_t j, bool ascending) {
        if (i >= n || j >= n) return;
        const bool should_swap = ascending ? (vec[i].t_hit > vec[j].t_hit)
                                           : (vec[i].t_hit < vec[j].t_hit);
        if (should_swap) {
            utility::swap(vec[i], vec[j]);
        }
    };

    // Find next power of 2 >= n
    const size_t pow2 = common::math::next_power_of_2(n);

    for (size_t k = 2; k <= pow2; k *= 2) {
        for (size_t j = k / 2; j > 0; j /= 2) {
            for (size_t i = 0; i < pow2; i++) {
                const size_t ixj = i ^ j;
                if (ixj > i) {
                    const bool ascending = ((i & k) == 0);
                    compare_and_swap(i, ixj, ascending);
                }
            }
        }
    }
}

// ============================================================================
// Adaptive Sort Dispatcher
// ============================================================================
// Automatically selects the best sorting algorithm based on array size
template <size_t N>
__device__ __forceinline__ void sort(utils::StaticVector<HitRecord, N>& vec) {
    const size_t n = vec.size();

    if (n <= 1) {
        return;  // Already sorted
    } else if (n <= 64) {
        // Insertion sort: O(n²) but excellent cache behavior and low overhead
        // NOTE: Warp shuffle sort would be faster but requires warp cooperation
        // which doesn't work when each thread has its own independent vector
        insertion_sort(vec);
    } else {
        // Bitonic sort: O(n log²n) parallel sorting network
        // Works well for larger arrays, single-threaded (no warp cooperation needed)
        bitonic_sort(vec);
    }
}

} // namespace device
} // namespace thesis
