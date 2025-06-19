#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/result.h"

#include <cuda_runtime_api.h>

#include <functional>

namespace thesis::host::cuda {

class Stream {
   private:
    cudaStream_t stream_ = nullptr;
    cudaEvent_t event_ = nullptr;
    // TODO(kacper): store last error

    using Callback = std::function<utils::Result<>(cudaError_t)>;

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

    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    [[nodiscard]] cudaEvent_t event() const noexcept { return event_; }
    [[nodiscard]] cudaStream_t get() const noexcept { return stream_; }

    void synchronize() const { CUDA_CHECK(cudaStreamSynchronize(stream_)); }
    static void synchronizeDevice() { CUDA_CHECK(cudaDeviceSynchronize()); }

    void recordEvent() { CUDA_CHECK(cudaEventRecord(event_, stream_)); }

    template <typename F>
    void addCallback(F&& func) {
        auto* cb_ptr = new Callback(std::forward<F>(func));

        CUDA_CHECK(cudaStreamAddCallback(
            stream_,
            [](cudaStream_t, cudaError_t status, void* userData) {
                CUDA_CHECK_NOEXCEPT(status);

                auto* cb = static_cast<Callback*>(userData);
                auto result = (*cb)(status);
                if (!result) {
                    spdlog::error("Stream callback failed: {}", result.error());
                }
                delete cb;
            },
            static_cast<void*>(cb_ptr), 0));
    }

   private:
    void reset() noexcept {
        if (stream_) {
            CUDA_CHECK_NOEXCEPT(cudaStreamDestroy(stream_));
            stream_ = nullptr;

            CUDA_CHECK_NOEXCEPT(cudaEventDestroy(event_));
            event_ = nullptr;
        }
    }
};

}  // namespace thesis::host::cuda
