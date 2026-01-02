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

#ifdef DEVICE
    __device__ __forceinline__ T* data() { return data_; }
    __device__ __forceinline__ const T* data() const { return data_; }
    __device__ __forceinline__ size_t capacity() const { return Capacity; }
#endif  // DEVICE
};

template <typename T>
struct DynamicStorage {
    size_t capacity_ = 0;
    T* data_ = nullptr;

    DynamicStorage(T* ptr, size_t cap)
        : capacity_(cap),
          data_(ptr) {}
    DynamicStorage() = default;
    DynamicStorage(const DynamicStorage&) = default;
    DynamicStorage& operator=(const DynamicStorage&) = default;
    DynamicStorage& operator=(DynamicStorage&&) = default;
    DynamicStorage(DynamicStorage&&) = default;

#ifdef DEVICE
    __device__ __forceinline__ T* data() { return data_; }
    __device__ __forceinline__ const T* data() const { return data_; }
    __device__ __forceinline__ size_t capacity() const { return capacity_; }
#endif  // DEVICE
};

template <typename T, typename Storage>
class VectorBase : private Storage {
   protected:
    size_t size_ = 0;

   public:
#ifdef DEVICE
    using Storage::capacity;
    using Storage::data;
#endif  // DEVICE

    VectorBase() = default;
    VectorBase(const VectorBase&) = default;
    VectorBase& operator=(const VectorBase&) = default;
    VectorBase(T* ptr, size_t size)
        : Storage(ptr, size),
          size_(size) {}

#ifdef DEVICE
    __device__ __forceinline__ size_t size() const { return size_; }
    __device__ __forceinline__ bool empty() const { return size_ == 0; }
    __device__ __forceinline__ bool full() const { return size_ == capacity(); }

    __device__ __forceinline__ T& operator[](size_t i) { return data()[i]; }
    __device__ __forceinline__ const T& operator[](size_t i) const { return data()[i]; }

    __device__ __forceinline__ void clear() { size_ = 0; }

    __device__ __forceinline__ bool resize(size_t new_size) {
        if (new_size > capacity()) {
            return false;
        }
        size_ = new_size;
        return true;
    }

    __device__ __forceinline__ bool push_back(const T& value) {
        if (full()) {
            return false;
        }
        data()[size_++] = value;
        return true;
    }

    template <typename... Args>
    __device__ __forceinline__ bool emplace_back(Args&&... args) {
        if (full()) {
            return false;
        }
        data()[size_++] = T(utility::forward<Args>(args)...);
        return true;
    }

    __device__ __forceinline__ Optional<T> pop_back() {
        return empty() ? utils::nullopt : data()[--size_];
    }

    __device__ __forceinline__ T* begin() { return data(); }
    __device__ __forceinline__ T* end() { return data() + size_; }

    __device__ __forceinline__ const T* begin() const { return data(); }
    __device__ __forceinline__ const T* end() const { return data() + size_; }
#endif  // DEVICE
};

template <typename T, size_t Capacity>
using StaticVector = VectorBase<T, StaticStorage<T, Capacity>>;

template <typename T>
using DynamicVector = VectorBase<T, DynamicStorage<T>>;

}  // namespace utils
}  // namespace device
}  // namespace thesis