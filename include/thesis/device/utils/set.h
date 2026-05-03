#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/utility.h"

#include <cstddef>
#include <type_traits>

namespace thesis {
namespace device {
namespace utils {

// ------------------------------------------------------------------------------
//                               SET BASE CLASS
// ------------------------------------------------------------------------------

template <typename T, size_t Capacity, typename Policy>
class THESIS_ALIGNMENT SetBase {
    static_assert(Capacity > 0, "Capacity must be > 0");

   private:
    friend Policy;  // give chosen Policy full access
    T data_[Capacity];
    size_t size_ = 0;

   public:
#ifdef __CUDA_ARCH__
    // basic query API
    __device__ __forceinline__ size_t size() const { return size_; }
    __device__ __forceinline__ constexpr size_t capacity() const { return Capacity; }
    __device__ __forceinline__ bool empty() const { return size_ == 0; }
    __device__ __forceinline__ bool full() const { return size_ == Capacity; }

    // modifiers delegated to policies
    __device__ __forceinline__ void clear() { size_ = 0; }

    __device__ __forceinline__ bool contains(const T& v) const {
        return Policy::contains(*this, v);
    }
    __device__ __forceinline__ bool insert(const T& v) { return Policy::insert(*this, v); }
    __device__ __forceinline__ bool erase(const T& v) { return Policy::erase(*this, v); }

    // Initialize from pre-sorted contiguous array (replaces existing contents)
    // REQUIRES: src must be sorted if this is a BinarySet, unsorted OK for LinearSet
    // Returns false if insufficient capacity, true on success
    __device__ __forceinline__ bool init_from_presorted(const T* src, size_t count) {
        if (count > Capacity) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            data_[i] = src[i];
        }
        size_ = count;
        return true;
    }

    // iterators
    __device__ __forceinline__ T* begin() { return data_; }
    __device__ __forceinline__ T* end() { return data_ + size_; }
    __device__ __forceinline__ const T* begin() const { return data_; }
    __device__ __forceinline__ const T* end() const { return data_ + size_; }

    // indexed access (no bounds checking)
    __device__ __forceinline__ T& operator[](size_t i) { return data_[i]; }
    __device__ __forceinline__ const T& operator[](size_t i) const { return data_[i]; }
#endif  // DEVICE
};

// ------------------------------------------------------------------------------
//                                   POLICIES
// ------------------------------------------------------------------------------
namespace detail {

// --- Linear (unsorted) policy --------------------------------------------

template <typename T, size_t Capacity>
struct LinearSetPolicy {
#ifdef __CUDA_ARCH__
    template <typename Base>
    __device__ static bool contains(const Base& s, const T& value) {
        for (size_t i = 0; i < s.size_; ++i) {
            if (s.data_[i] == value) {
                return true;
            }
        }
        return false;
    }

    template <typename Base>
    __device__ static bool insert(Base& s, const T& value) {
        if (contains(s, value) || s.full()) {
            return false;
        }

        s.data_[s.size_++] = value;
        return true;
    }

    template <typename Base>
    __device__ static bool erase(Base& s, const T& value) {
        for (size_t i = 0; i < s.size_; ++i) {
            if (s.data_[i] == value) {
                s.data_[i] = s.data_[--s.size_];
                return true;
            }
        }
        return false;
    }
#endif  // DEVICE
};

// --- Binary (sorted) policy --------------------------------------------

template <typename T, size_t Capacity>
class BinarySetPolicy {
#ifdef __CUDA_ARCH__
    template <typename Base>
    __device__ static int lower_bound(const Base& s, const T& value) {
        int lo = -1;
        int hi = static_cast<int>(s.size_);

        while (hi - lo > 1) {
            int mid = (lo + hi) / 2;
            if (s.data_[mid] < value) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        return hi;
    }

    template <typename Base>
    __device__ static bool contains_at_idx(const Base& s, int idx, const T& value) {
        return idx < static_cast<int>(s.size_) && s.data_[idx] == value;
    }

   public:
    template <typename Base>
    __device__ static bool contains(const Base& s, const T& value) {
        int idx = lower_bound(s, value);
        return contains_at_idx(s, idx, value);
    }

    template <typename Base>
    __device__ static bool insert(Base& s, const T& value) {
        if (s.full()) {
            return false;
        }

        int idx = lower_bound(s, value);
        if (contains_at_idx(s, idx, value)) {
            return false;
        }

        // Shift right to make room
        for (int i = static_cast<int>(s.size_); i > idx; --i) {
            s.data_[i] = s.data_[i - 1];
        }
        s.data_[idx] = value;
        ++s.size_;

        return true;
    }

    template <typename Base>
    __device__ static bool erase(Base& s, const T& value) {
        int idx = lower_bound(s, value);
        if (!contains_at_idx(s, idx, value)) {
            return false;
        }

        // Shift right to make room
        --s.size_;
        for (int i = idx; i < static_cast<int>(s.size_); ++i) {
            s.data_[i] = s.data_[i + 1];
        }

        return true;
    }
#endif  // DEVICE
};

}  // namespace detail

// ------------------------------------------------------------------------------
//                              PUBLIC ALIASES
// ------------------------------------------------------------------------------

template <typename T, size_t Capacity>
using LinearSet = SetBase<T, Capacity, detail::LinearSetPolicy<T, Capacity>>;

template <typename T, size_t Capacity>
using BinarySet = SetBase<T, Capacity, detail::BinarySetPolicy<T, Capacity>>;

constexpr size_t SET_THRESHOLD = 32;

template <typename T, size_t Capacity>
using Set =
    std::conditional_t<(Capacity > SET_THRESHOLD), BinarySet<T, Capacity>, LinearSet<T, Capacity>>;

// Helper constant: true if Set<T, Capacity> uses BinarySet (requires sorted order)
template <size_t Capacity>
constexpr bool set_requires_sorting = (Capacity > SET_THRESHOLD);

}  // namespace utils
}  // namespace device
}  // namespace thesis
