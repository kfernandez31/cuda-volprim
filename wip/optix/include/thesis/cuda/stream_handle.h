#pragma once

#ifdef __cplusplus

#include "thesis/utils/check.h"

#include <cuda_runtime_api.h>

#include <utility>

namespace thesis::cuda {

class StreamHandle {
public:
    explicit StreamHandle(unsigned int flags = cudaStreamDefault)
    {
        CUDA_CHECK(cudaStreamCreateWithFlags(&stream_, flags));
    }

    ~StreamHandle()
    {
        CUDA_CHECK_NOEXCEPT(cudaStreamDestroy(stream_));
    }

    // Disable copy
    StreamHandle(const StreamHandle&) = delete;
    StreamHandle& operator=(const StreamHandle&) = delete;

    // Enable move ctor
    StreamHandle(StreamHandle&& other) noexcept
        : stream_(std::exchange(other.stream_, nullptr)) {}

    // Enable move assignment
    StreamHandle& operator=(StreamHandle&& other) noexcept {
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

} // namespace thesis::cuda

#endif // __cplusplus
