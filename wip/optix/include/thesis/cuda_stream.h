#pragma once

#ifdef __cplusplus

#include "thesis/check.h"

#include <cuda_runtime_api.h>

#include <utility>

namespace thesis {

class CudaStreamHandle {
public:
    explicit CudaStreamHandle(unsigned int flags = cudaStreamDefault)
    {
        CUDA_CHECK(cudaStreamCreateWithFlags(&stream_, flags));
    }

    ~CudaStreamHandle()
    {
        CUDA_CHECK_NOEXCEPT(cudaStreamDestroy(stream_));
    }

    // Disable copy
    CudaStreamHandle(const CudaStreamHandle&) = delete;
    CudaStreamHandle& operator=(const CudaStreamHandle&) = delete;

    // Enable move ctor
    CudaStreamHandle(CudaStreamHandle&& other) noexcept
        : stream_(std::exchange(other.stream_, nullptr)) {}

    // Enable move assignment
    CudaStreamHandle& operator=(CudaStreamHandle&& other) noexcept {
        if (this != &other) {
            CUDA_CHECK_NOEXCEPT(cudaStreamDestroy(stream_));

            stream_ = std::exchange(other.stream_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] const cudaStream_t& get() const noexcept { return stream_; }
    [[nodiscard]]       cudaStream_t& get()       noexcept { return stream_; }

    void synchronize() const
    {
        CUDA_CHECK(cudaStreamSynchronize(stream_));
    }

    static void synchronizeDevice()
    {
        CUDA_CHECK(cudaDeviceSynchronize());
    }

private:
    cudaStream_t stream_ = nullptr;
};

} // namespace thesis

#endif // __cplusplus
