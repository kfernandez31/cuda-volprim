#pragma once

#ifdef __cplusplus

#include "thesis/check.h"

#include <cuda.h>

#include <utility>

namespace thesis {

template <typename T>
class CudaUpload {
public:
    explicit CudaUpload(const T& value)
    {
        CU_CHECK(cuMemAlloc(&device_ptr_, sizeof(T)));
        CU_CHECK(cuMemcpyHtoD(device_ptr_, &value, sizeof(T)));
    }

    ~CudaUpload() noexcept
    {
        CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
    }

    CudaUpload(const CudaUpload&) = delete;
    CudaUpload& operator=(const CudaUpload&) = delete;

    CudaUpload(CudaUpload&& other) noexcept
        : device_ptr_(std::exchange(other.device_ptr_, 0)) {}

    CudaUpload& operator=(CudaUpload&& other) noexcept
    {
        if (this == &other) {
            CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
        }
        return *this;
    }

    [[nodiscard]] const CUdeviceptr& get() const noexcept { return device_ptr_; }
    [[nodiscard]]       CUdeviceptr& get()       noexcept { return device_ptr_; }

private:
    CUdeviceptr device_ptr_ = 0;
};

} // namespace thesis

#endif // __cplusplus
