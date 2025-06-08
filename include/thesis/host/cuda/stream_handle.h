#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/utils/check.h"

#include <cuda_runtime_api.h>

#include <utility>

namespace thesis::host::cuda {

class StreamHandle {
   private:
    cudaStream_t stream_ = nullptr;

    void reset() noexcept {
        if (stream_) {
            CUDA_CHECK_NOEXCEPT(cudaStreamDestroy(stream_));
        }
    }

   public:
    explicit StreamHandle(uint flags = cudaStreamDefault) {
        CUDA_CHECK(cudaStreamCreateWithFlags(&stream_, flags));
    }

    ~StreamHandle() { reset(); }

    StreamHandle(StreamHandle&& other) noexcept : stream_(std::exchange(other.stream_, nullptr)) {}
    StreamHandle& operator=(StreamHandle&& other) noexcept {
        if (this != &other) {
            reset();
            stream_ = std::exchange(other.stream_, nullptr);
        }
        return *this;
    }

    StreamHandle(const StreamHandle&) = delete;
    StreamHandle& operator=(const StreamHandle&) = delete;

    [[nodiscard]] cudaStream_t get() const noexcept { return stream_; }

    void synchronize() const { CUDA_CHECK(cudaStreamSynchronize(stream_)); }
    static void synchronizeDevice() { CUDA_CHECK(cudaDeviceSynchronize()); }
};

}  // namespace thesis::host::cuda
