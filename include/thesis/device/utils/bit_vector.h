#pragma once

#include <cstdint>

namespace thesis {
namespace device {
namespace utils {

// Templated bit vector supporting up to N elements (N must be multiple of 64)
// Uses cached size to avoid recomputing popcnt on every size() call
//
// Memory usage: ceil(N/64) × 8 bytes + 2 bytes = (N/8 + 2) bytes
// Example: BitVector<1024> = 128 + 2 = 130 bytes (vs ~4100 for Set<uint, 1024>)
template<size_t N>
struct BitVector {
    static_assert((N & 63) == 0, "Capacity must be multiple of 64 for efficient word packing");
    static constexpr size_t NUM_WORDS = N >> 6;

    uint64_t bits_[NUM_WORDS];
    size_t size_;  // Cached size (avoids recomputing popcnt)

#ifdef DEVICE
    __device__ __forceinline__ BitVector() : size_(0) {
        #pragma unroll
        for (int i = 0; i < NUM_WORDS; ++i) {
            bits_[i] = 0;
        }
    }

    __device__ __forceinline__ void clear() {
        #pragma unroll
        for (int i = 0; i < NUM_WORDS; ++i) {
            bits_[i] = 0;
        }
        size_ = 0;
    }

    __device__ __forceinline__ void insert(uint idx) {
#ifdef DEBUG
        if (idx >= N) {
            printf("ERROR: BitVector::insert(%u) out of bounds (N=%zu)\n", idx, N);
            return;
        }
#endif // DEBUG
        const uint word = idx >> 6;   // Divide by 64
        const uint bit = idx & 63;    // Modulo 64
        const uint64_t mask = 1ULL << bit;

        // Only increment size if bit wasn't already set (avoid double-counting)
        if (!(bits_[word] & mask)) {
            bits_[word] |= mask;
            ++size_;
        }
    }

    __device__ __forceinline__ bool contains(uint idx) const {
#ifdef DEBUG
        if (idx >= N) {
            printf("ERROR: BitVector::contains(%u) out of bounds (N=%zu)\n", idx, N);
            return false;
        }
#endif // DEBUG
        const uint word = idx >> 6;   // Divide by 64
        const uint bit = idx & 63;    // Modulo 64
        return bits_[word] & (1ULL << bit);
    }

    __device__ __forceinline__ void erase(uint idx) {
#ifdef DEBUG
        if (idx >= N) {
            printf("ERROR: BitVector::erase(%u) out of bounds (N=%zu)\n", idx, N);
            return;
        }
#endif // DEBUG
        const uint word = idx >> 6;   // Divide by 64
        const uint bit = idx & 63;    // Modulo 64
        const uint64_t mask = 1ULL << bit;

        // Only decrement size if bit was set
        if (bits_[word] & mask) {
            bits_[word] &= ~mask;
            --size_;
        }
    }

    // O(1) cached size - no recomputation needed!
    __device__ __forceinline__ uint size() const {
        return size_;
    }

    __device__ __forceinline__ bool empty() const {
        return size_ == 0;
    }

    static constexpr size_t capacity() {
        return N;
    }

    // Range-based for loop support - iterate over set bits
    // Iterator complexity:
    // - Construction: O(NUM_WORDS) worst case (sparse bits, finds first set bit)
    // - operator++: O(NUM_WORDS) worst case (finds next set bit)
    // - operator*: O(1)
    // Best for iterating over moderately populated sets (>10% fill rate)
    // WARNING: Not thread-safe. Each thread must own its own BitVector instance.
    struct Iterator {
        const BitVector* vec_;
        size_t word_;
        uint64_t remaining_;

        __device__ __forceinline__ Iterator(const BitVector* vec, size_t word)
            : vec_(vec), word_(word), remaining_(0) {
            if (word_ < NUM_WORDS) {
                remaining_ = vec_->bits_[word_];
                advance_to_next_set_bit();
            }
        }

        __device__ __forceinline__ void advance_to_next_set_bit() {
            // Advance until we find a set bit or reach the end
            while (remaining_ == 0 && word_ < NUM_WORDS - 1) {
                ++word_;
                remaining_ = vec_->bits_[word_];
            }
            // If still no bits set, move to end position
            if (remaining_ == 0) {
                word_ = NUM_WORDS;
            }
        }

        __device__ __forceinline__ uint operator*() const {
#ifdef DEBUG
            // Should never dereference iterator with no remaining bits
            if (remaining_ == 0) {
                printf("ERROR: Dereferencing BitVector iterator with remaining_=0\n");
            }
#endif // DEBUG
            // __ffsll returns 1-indexed position, or 0 if no bits set
            // Subtracting 1 gives 0-indexed position
            const uint bit = __ffsll(remaining_) - 1;
            return (word_ << 6) + bit;  // Multiply word by 64, add bit position
        }

        __device__ __forceinline__ Iterator& operator++() {
            remaining_ &= remaining_ - 1;  // Clear lowest set bit
            advance_to_next_set_bit();
            return *this;
        }

        __device__ __forceinline__ bool operator!=(const Iterator& other) const {
            // End iterator has word_ == NUM_WORDS
            // Compare positions, not bit patterns
            if (word_ != other.word_) return true;
            if (word_ >= NUM_WORDS) return false;  // Both at end
            return remaining_ != other.remaining_;
        }
    };

    __device__ __forceinline__ Iterator begin() const {
        return Iterator(this, 0);
    }

    __device__ __forceinline__ Iterator end() const {
        return Iterator(this, NUM_WORDS);
    }

    // Initialize from array (bulk insert)
    // Note: For BitVector, order doesn't matter (unlike sorted arrays)
    __device__ __forceinline__ void init_from_array(const uint* src, size_t count) {
        clear();
        for (size_t i = 0; i < count; ++i) {
            insert(src[i]);
        }
    }

    // DEPRECATED: Use init_from_array() instead
    // Kept for backward compatibility with existing code
    __device__ __forceinline__ void init_from_presorted(const uint* src, size_t count) {
        init_from_array(src, count);
    }
#endif  // DEVICE
};

// Type aliases for common sizes (all multiples of 64)
using BitVector64 = BitVector<64>;
using BitVector128 = BitVector<128>;
using BitVector256 = BitVector<256>;
using BitVector512 = BitVector<512>;
using BitVector1024 = BitVector<1024>;
using BitVector2048 = BitVector<2048>;

}  // namespace utils
}  // namespace device
}  // namespace thesis
