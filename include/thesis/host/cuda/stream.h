#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/utils/check.h"

#include <cuda_runtime_api.h>

namespace thesis::host::cuda {

class Stream {
   private:
    cudaStream_t stream_ = nullptr;
    cudaEvent_t event_ = nullptr;

   public:
    explicit Stream(bool is_default) {
        CUDA_CHECK(cudaStreamCreateWithFlags(
            &stream_, is_default ? cudaStreamDefault : cudaStreamNonBlocking));
        CUDA_CHECK(cudaEventCreateWithFlags(&event_, cudaEventDisableTiming));
    }

    ~Stream() { reset(); }

    Stream(Stream&& other) noexcept
        : stream_(std::exchange(other.stream_, nullptr)),
          event_(std::exchange(other.event_, nullptr)) {}

    Stream& operator=(Stream&& other) noexcept {
        if (this != &other) {
            reset();
            stream_ = std::exchange(other.stream_, nullptr);
            event_ = std::exchange(other.event_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] cudaEvent_t event() const noexcept { return event_; }
    [[nodiscard]] cudaStream_t get() const noexcept { return stream_; }

    void synchronize() const { CUDA_CHECK(cudaStreamSynchronize(stream_)); }
    static void synchronizeDevice() { CUDA_CHECK(cudaDeviceSynchronize()); }

    void recordEvent() { CUDA_CHECK(cudaEventRecord(event_, stream_)); }

   private:
    void reset() noexcept {
        if (stream_) {
            CUDA_CHECK_NOEXCEPT(cudaStreamDestroy(stream_));
            stream_ = nullptr;
        }

        if (event_) {
            CUDA_CHECK_NOEXCEPT(cudaEventDestroy(event_));
            event_ = nullptr;
        }
    }
};

}  // namespace thesis::host::cuda
