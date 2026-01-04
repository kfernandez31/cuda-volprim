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
            const HitRecord other = HitRecord{__shfl_sync(0xFFFFFFFF, my_record.t_hit, ixj),
                                              __shfl_sync(0xFFFFFFFF, my_record.prim_idx, ixj),
                                              __shfl_sync(0xFFFFFFFF, my_record.is_exit, ixj)};

            const bool ascending = ((lane_id & k) == 0);
            const bool should_swap =
                (ascending ? (my_record.t_hit > other.t_hit) : (my_record.t_hit < other.t_hit));

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
// Bitonic Sort (for n > 64)
// ============================================================================
// O(n log²n) parallel sorting network
// Works on power-of-2 capacity by padding with sentinels
// Helper: Initialize buffer with sentinels (call once after construction)
template <size_t N>
__device__ __forceinline__ void init_hit_buffer_sentinels(utils::StaticVector<HitRecord, N>& vec) {
    if constexpr ((N & (N - 1)) != 0) {
        // no-op for non-power-of-2 capacities (compiler eliminates entire function)
        return;
    }

    // Capacity is power of 2: pre-fill entire capacity with sentinels
    // This enables zero-overhead bitonic sort (just resize, no padding loop)
    // Cost: 128 writes once per buffer, but enables multiple sorts if needed
    const HitRecord sentinel{consts::INF_F, 0u, false};
    vec.clear();
    for (size_t i = 0; i < N; i++) {
        vec.push_back(sentinel);
    }
    vec.clear();  // Reset size to 0, but all slots now contain sentinels
}

template <size_t N>
__device__ void bitonic_sort(utils::StaticVector<HitRecord, N>& vec) {
    static_assert((N & (N - 1)) == 0, "Capacity must be power of 2 for bitonic sort");

    const size_t original_size = vec.size();

    // Extend to full capacity (assumes buffer was pre-initialized with sentinels, see:
    // init_hit_buffer_sentinels)
    vec.resize(N);

    // Standard bitonic sort on power-of-2 array
    for (size_t k = 2; k <= N; k *= 2) {
        for (size_t j = k / 2; j > 0; j /= 2) {
            for (size_t i = 0; i < N; i++) {
                const size_t ixj = i ^ j;
                if (ixj <= i)
                    continue;

                const bool ascending = ((i & k) == 0);

                HitRecord a = vec[i];
                HitRecord b = vec[ixj];

                const bool a_less = a < b;
                HitRecord mn = a_less ? a : b;
                HitRecord mx = a_less ? b : a;

                vec[i] = ascending ? mn : mx;
                vec[ixj] = ascending ? mx : mn;
            }
        }
    }

    // Restore original size (sentinels now at end after sorting)
    vec.resize(original_size);
}

// ============================================================================
// Adaptive Sort Dispatcher
// ============================================================================
// Automatically selects sorting algorithm based on size and capacity
template <size_t N>
__device__ __forceinline__ void sort(utils::StaticVector<HitRecord, N>& vec) {
    const size_t n = vec.size();

    if (n <= 1) {
        return;  // Already sorted
    }

    if (n <= 64) {  // small arrays
        insertion_sort(vec);
    } else {                                 // big arrays
        if constexpr ((N & (N - 1)) == 0) {  // power-of-2 capacity
            bitonic_sort(vec);
        } else {
            insertion_sort(vec);
        }
    }
}

}  // namespace device
}  // namespace thesis
