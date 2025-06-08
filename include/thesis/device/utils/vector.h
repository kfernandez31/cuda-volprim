#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/device/utils/optional.h"
#include "thesis/device/utils/utility.h"

#include <cstddef>

namespace thesis {
namespace device {
namespace utils {

template <typename T, size_t Capacity>
struct StaticStorage {
    T data_[Capacity];

    THESIS_INLINE THESIS_HOST_DEVICE T* data() noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE const T* data() const noexcept { return data_; }

    static constexpr size_t capacity = Capacity;
};

template <typename T>
struct DynamicStorage {
    size_t capacity_ = 0;
    T* data_ = nullptr;

    DynamicStorage() = default;

    THESIS_HOST_DEVICE DynamicStorage(T* ptr, size_t cap) : capacity_(cap), data_(ptr) {}

    DynamicStorage(const DynamicStorage&) = default;
    DynamicStorage& operator=(const DynamicStorage&) = default;

    THESIS_HOST_DEVICE DynamicStorage(DynamicStorage&& other) noexcept
        : capacity_(utility::exchange(other.capacity_, 0)),
          data_(utility::exchange(other.data_, nullptr)) {}

    THESIS_HOST_DEVICE DynamicStorage& operator=(DynamicStorage&& other) noexcept {
        if (this != &other) {
            capacity_ = utility::exchange(other.capacity_, 0);
            data_ = utility::exchange(other.data_, nullptr);
        }
        return *this;
    }

    THESIS_INLINE THESIS_HOST_DEVICE T* data() noexcept { return data_; }
    THESIS_INLINE THESIS_HOST_DEVICE const T* data() const noexcept { return data_; }

    THESIS_INLINE THESIS_HOST_DEVICE size_t capacity() const noexcept { return capacity_; }
};

template <typename T, typename Storage>
class VectorBase : private Storage {
   protected:
    size_t size_ = 0;

   public:
    using Storage::capacity;
    using Storage::data;

    VectorBase() = default;

    THESIS_HOST_DEVICE explicit VectorBase(Storage&& s) : Storage(utility::move(s)) {}

    VectorBase(const VectorBase&) = default;
    VectorBase& operator=(const VectorBase&) = default;

    THESIS_HOST_DEVICE VectorBase(VectorBase&& other) noexcept
        : Storage(utility::move(other)), size_(utility::exchange(other.size_, 0)) {}

    THESIS_HOST_DEVICE VectorBase& operator=(VectorBase&& other) noexcept {
        if (this != &other) {
            static_cast<Storage&>(*this) = utility::move(static_cast<Storage&>(other));
            size_ = utility::exchange(other.size_, 0);
        }
        return *this;
    }

    THESIS_INLINE THESIS_HOST_DEVICE size_t size() const noexcept { return size_; }
    THESIS_INLINE THESIS_HOST_DEVICE bool empty() const noexcept { return size_ == 0; }
    THESIS_INLINE THESIS_HOST_DEVICE bool full() const noexcept { return size_ == capacity(); }

    THESIS_INLINE THESIS_HOST_DEVICE T& operator[](size_t i) noexcept { return data()[i]; }
    THESIS_INLINE THESIS_HOST_DEVICE const T& operator[](size_t i) const noexcept {
        return data()[i];
    }

    THESIS_INLINE THESIS_HOST_DEVICE void clear() noexcept { size_ = 0; }

    THESIS_INLINE THESIS_HOST_DEVICE bool push_back(const T& value) noexcept {
        if (full()) {
            return false;
        }
        data()[size_++] = value;
        return true;
    }

    template <typename... Args>
    THESIS_INLINE THESIS_HOST_DEVICE bool emplace_back(Args&&... args) noexcept {
        if (full()) {
            return false;
        }
        data()[size_++] = T(utility::forward<Args>(args)...);
        return true;
    }

    THESIS_INLINE THESIS_HOST_DEVICE Optional<T> pop_back() noexcept {
        if (empty())
            return utils::nullopt;
        return data()[--size_];
    }

    THESIS_INLINE THESIS_HOST_DEVICE T* begin() noexcept { return data(); }
    THESIS_INLINE THESIS_HOST_DEVICE T* end() noexcept { return data() + size_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T* begin() const noexcept { return data(); }
    THESIS_INLINE THESIS_HOST_DEVICE const T* end() const noexcept { return data() + size_; }

    THESIS_INLINE THESIS_HOST_DEVICE const T* cbegin() const noexcept { return data(); }
    THESIS_INLINE THESIS_HOST_DEVICE const T* cend() const noexcept { return data() + size_; }
};

template <typename T, size_t Capacity>
using StaticVector = VectorBase<T, StaticStorage<T, Capacity>>;

template <typename T>
struct DynamicVector : VectorBase<T, DynamicStorage<T>> {
    using Base = VectorBase<T, DynamicStorage<T>>;
    using Base::Base;

    THESIS_HOST_DEVICE DynamicVector(T* ptr, size_t cap) : Base(DynamicStorage<T>{ptr, cap}) {}
};

}  // namespace utils
}  // namespace device
}  // namespace thesis
