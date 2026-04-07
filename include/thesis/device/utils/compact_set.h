#pragma once

#include <cstddef>
#include <cstdint>

namespace thesis {
namespace device {
namespace utils {

// Fixed-capacity set stored as an unsorted array of values.
// Capacity bounds the max number of *simultaneously active* elements,
// NOT the range of possible values — so it decouples from scene size.
//
// Memory: Capacity * sizeof(T) + sizeof(size_t) bytes
// Example: CompactSet<uint16_t, 64> = 64*2 + 8 = 136 bytes
//
// All operations are O(k) where k = current size (typically 1-10).
// For small k this beats BitVector's O(N/64) iteration.
template <typename T, size_t Capacity>
struct CompactSet {
    T data_[Capacity];
    size_t size_;

#ifdef DEVICE
    __device__ __forceinline__ CompactSet()
        : size_(0) {}

    __device__ __forceinline__ void clear() { size_ = 0; }

    __device__ __forceinline__ void insert(unsigned int idx) {
        if (size_ >= Capacity || contains(idx))
            return;
        data_[size_++] = static_cast<T>(idx);
    }

    __device__ __forceinline__ bool contains(unsigned int idx) const {
        const auto val = static_cast<T>(idx);
        for (size_t i = 0; i < size_; ++i) {
            if (data_[i] == val)
                return true;
        }
        return false;
    }

    __device__ __forceinline__ void erase(unsigned int idx) {
        const auto val = static_cast<T>(idx);
        for (size_t i = 0; i < size_; ++i) {
            if (data_[i] == val) {
                data_[i] = data_[--size_];  // swap with last
                return;
            }
        }
    }

    __device__ __forceinline__ size_t size() const { return size_; }
    __device__ __forceinline__ bool empty() const { return size_ == 0; }
    static constexpr size_t capacity() { return Capacity; }

    // Range-based for: iterates only over active entries
    __device__ __forceinline__ const T* begin() const { return data_; }
    __device__ __forceinline__ const T* end() const { return data_ + size_; }

    // Bulk init from array (no duplicate check — caller must ensure uniqueness)
    template <typename IndexT>
    __device__ __forceinline__ void init_from_array(const IndexT* src, size_t count) {
        size_ = count < Capacity ? count : Capacity;
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = static_cast<T>(src[i]);
        }
    }
#endif  // DEVICE
};

}  // namespace utils
}  // namespace device
}  // namespace thesis
