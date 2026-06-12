#pragma once

#include "thesis/common/utils/types.h"

#include <cstdint>

namespace thesis {
namespace device {

// SoA entry-hit buffer for the primary/scatter argmin path.
//
// The argmin estimator stores ONLY entry hits (anyhit COLLECT mode), never sorts
// them (the min is order-independent), and never distinguishes entries from exits.
// So the old AoS `HitRecord { float; prim_idx_t; prim_idx_t is_exit:1; }` carried a
// whole 2-byte word for a 1-bit flag that was always false and never read, and the
// embedded float forced the struct to 8 bytes (4-byte alignment + padding).
//
// Splitting into parallel arrays drops the dead exit word and the alignment padding,
// shrinking per-entry storage 8B → 6B (float 4B + prim_idx_t 2B) — a 25% cut on the
// dominant per-ray local-memory buffer (HIT_BUFFER_CAPACITY entries). This is the
// per-ray-state footprint reduction the §8.28 ncu profile pointed at: smaller local
// footprint → better L1 residency → fewer long_scoreboard (memory-latency) stalls.
// Bit-identical: only the storage layout changes; the stored (t, prim_idx) values and
// their iteration order are unchanged.
template <size_t Capacity>
struct HitBufferSoA {
    float t_hit_[Capacity];          // 4B/entry — distance along ray
    prim_idx_t prim_idx_[Capacity];  // 2B/entry — primitive index
    size_t size_ = 0;

    // Total COLLECT-anyhit invocations this trace — counts EVERY entry hit, including
    // those dropped on overflow, so --measure-caps can read true per-ray demand from
    // any binary regardless of its compiled cap (the anyhit always fires; only the
    // push is capped). One local-memory add per hit; observation-only.
    uint32_t total_seen_ = 0;

#ifdef __CUDA_ARCH__
    __device__ __forceinline__ void clear() { size_ = 0; total_seen_ = 0; }
    __device__ __forceinline__ size_t size() const { return size_; }
    __device__ __forceinline__ bool empty() const { return size_ == 0; }
    __device__ __forceinline__ bool full() const { return size_ == Capacity; }
    static constexpr size_t capacity() { return Capacity; }

    // Append an entry hit. Returns false (dropping it) if at capacity, mirroring the
    // old StaticVector::emplace_back contract so the anyhit's overflow path is unchanged.
    __device__ __forceinline__ bool push(float t, prim_idx_t idx) {
        if (size_ >= Capacity)
            return false;
        t_hit_[size_] = t;
        prim_idx_[size_] = idx;
        ++size_;
        return true;
    }
#endif  // DEVICE
};

}  // namespace device
}  // namespace thesis
