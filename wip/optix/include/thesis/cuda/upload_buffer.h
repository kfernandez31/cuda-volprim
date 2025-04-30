#pragma once

#ifdef __cplusplus

#include "thesis/utils/check.h"

#include <cuda.h>

#include <utility>

namespace thesis::cuda {

template <typename T>
class UploadBuffer {
   public:
    UploadBuffer() = default;

    UploadBuffer(const T* data, size_t count) : count_(count) {
        CU_CHECK(cuMemAlloc(&device_ptr_, sizeof(T) * count));
        CU_CHECK(cuMemcpyHtoD(device_ptr_, data, sizeof(T) * count));
    }

    explicit UploadBuffer(const T& value) : UploadBuffer(&value, 1) {}

    static UploadBuffer createEmpty(size_t count) {
        UploadBuffer buffer;
        buffer.count_ = count;
        CU_CHECK(cuMemAlloc(&buffer.device_ptr_, sizeof(T) * count));
        return buffer;
    }

    ~UploadBuffer() noexcept {
        CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
    }

    UploadBuffer(const UploadBuffer&) = delete;
    UploadBuffer& operator=(const UploadBuffer&) = delete;

    UploadBuffer(UploadBuffer&& other) noexcept
        : device_ptr_(std::exchange(other.device_ptr_, 0)),
          count_(std::exchange(other.count_, 0)) {}

    UploadBuffer& operator=(UploadBuffer&& other) noexcept {
        if (this != &other) {
            CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
            count_ = std::exchange(other.count_, 0);
        }
        return *this;
    }

    [[nodiscard]] const CUdeviceptr& get() const noexcept { return device_ptr_; }
    [[nodiscard]] CUdeviceptr& get() noexcept { return device_ptr_; }

    [[nodiscard]] size_t size() const { return count_; }

   private:
    CUdeviceptr device_ptr_ = 0;
    size_t count_ = 0;
};

} // namespace thesis::cuda

#endif  // __cplusplus
