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
    // basic query API
    THESIS_INLINE THESIS_HOST_DEVICE size_t size() const noexcept { return size_; }
    THESIS_INLINE THESIS_HOST_DEVICE constexpr size_t capacity() const noexcept { return Capacity; }
    THESIS_INLINE THESIS_HOST_DEVICE bool empty() const noexcept { return size_ == 0; }
    THESIS_INLINE THESIS_HOST_DEVICE bool full() const noexcept { return size_ == Capacity; }

    // modifiers delegated to policies
    THESIS_INLINE THESIS_HOST_DEVICE void clear() noexcept { size_ = 0; }

    THESIS_INLINE THESIS_HOST_DEVICE bool contains(const T& v) const noexcept {
        return Policy::contains(*this, v);
    }
    THESIS_INLINE THESIS_HOST_DEVICE bool insert(const T& v) noexcept {
        return Policy::insert(*this, v);
    }
    THESIS_INLINE THESIS_HOST_DEVICE bool erase(const T& v) noexcept {
        return Policy::erase(*this, v);
    }

    // iterators
    THESIS_INLINE THESIS_HOST_DEVICE T* begin() noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE T* end() noexcept { return data_ + size_; }
    THESIS_INLINE THESIS_HOST_DEVICE const T* begin() const noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE const T* end() const noexcept { return data_ + size_; }
};

// ------------------------------------------------------------------------------
//                                   POLICIES
// ------------------------------------------------------------------------------
namespace detail {

// --- Linear (unsorted) policy --------------------------------------------

template <typename T, size_t Capacity>
struct LinearSetPolicy {
    template <typename Base>
    THESIS_INLINE THESIS_HOST_DEVICE static bool contains(const Base& s, const T& value) noexcept {
        for (size_t i = 0; i < s.size_; ++i) {
            if (s.data_[i] == value) {
                return true;
            }
        }
        return false;
    }

    template <typename Base>
    THESIS_INLINE THESIS_HOST_DEVICE static bool insert(Base& s, const T& value) noexcept {
        if (contains(s, value) || s.full())
            return false;

        s.data_[s.size_++] = value;
        return true;
    }

    template <typename Base>
    THESIS_INLINE THESIS_HOST_DEVICE static bool erase(Base& s, const T& value) noexcept {
        for (size_t i = 0; i < s.size_; ++i) {
            if (s.data_[i] == value) {
                s.data_[i] = utility::move(s.data_[--s.size_]);
                return true;
            }
        }
        return false;
    }
};

// --- Binary (sorted) policy --------------------------------------------

template <typename T, size_t Capacity>
class BinarySetPolicy {
    template <typename Base>
    THESIS_INLINE THESIS_HOST_DEVICE static int lower_bound(const Base& s,
                                                            const T& value) noexcept {
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
    THESIS_INLINE THESIS_HOST_DEVICE static bool contains_at_idx(const Base& s, int idx,
                                                                 const T& value) noexcept {
        return idx < static_cast<int>(s.size_) && s.data_[idx] == value;
    }

   public:
    template <typename Base>
    THESIS_INLINE THESIS_HOST_DEVICE static bool contains(const Base& s, const T& value) noexcept {
        int idx = lower_bound(s, value);
        return contains_at_idx(s, idx, value);
    }

    template <typename Base>
    THESIS_INLINE THESIS_HOST_DEVICE static bool insert(Base& s, const T& value) noexcept {
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
    THESIS_INLINE THESIS_HOST_DEVICE static bool erase(Base& s, const T& value) noexcept {
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
};

}  // namespace detail

// ------------------------------------------------------------------------------
//                              PUBLIC ALIASES
// ------------------------------------------------------------------------------

template <typename T, size_t Capacity>
using LinearSet = SetBase<T, Capacity, detail::LinearSetPolicy<T, Capacity>>;

template <typename T, size_t Capacity>
using BinarySet = SetBase<T, Capacity, detail::BinarySetPolicy<T, Capacity>>;

// automatic selection between linear and binary based on capacity
constexpr size_t SET_THRESHOLD = 32;

template <typename T, size_t Capacity>
using Set =
    std::conditional_t<(Capacity > SET_THRESHOLD), BinarySet<T, Capacity>, LinearSet<T, Capacity>>;

}  // namespace utils
}  // namespace device
}  // namespace thesis
