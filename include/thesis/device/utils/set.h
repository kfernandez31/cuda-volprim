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
#ifdef DEVICE
    // basic query API
    __device__ size_t size() const { return size_; }
    __device__ constexpr size_t capacity() const { return Capacity; }
    __device__ bool empty() const { return size_ == 0; }
    __device__ bool full() const { return size_ == Capacity; }

    // modifiers delegated to policies
    __device__ void clear() { size_ = 0; }

    __device__ bool contains(const T& v) const { return Policy::contains(*this, v); }
    __device__ bool insert(const T& v) { return Policy::insert(*this, v); }
    __device__ bool erase(const T& v) { return Policy::erase(*this, v); }

    // iterators
    __device__ T* begin() { return data_; }
    __device__ T* end() { return data_ + size_; }
    __device__ const T* begin() const { return data_; }
    __device__ const T* end() const { return data_ + size_; }
#endif  // DEVICE
};

// ------------------------------------------------------------------------------
//                                   POLICIES
// ------------------------------------------------------------------------------
namespace detail {

// --- Linear (unsorted) policy --------------------------------------------

template <typename T, size_t Capacity>
struct LinearSetPolicy {
#ifdef DEVICE
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
#ifdef DEVICE
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

}  // namespace utils
}  // namespace device
}  // namespace thesis
