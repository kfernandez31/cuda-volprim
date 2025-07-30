#pragma once

#include "thesis/host/cuda/stream.h"

#include <array>
#include <memory>
#include <spdlog/spdlog.h>

namespace thesis::host::cuda {

enum class StreamKind : size_t {
    Main = 0,
    GAS,
    IAS,
    EnvMap,
    Image,
    Prims,
    SBT,
    Count,
};

class StreamDAG {
   private:
    std::array<std::shared_ptr<Stream>, static_cast<size_t>(StreamKind::Count)> streams_;

   public:
    StreamDAG() {
        for (size_t i = 0; i < streams_.size(); ++i) {
            streams_[i] = std::make_shared<Stream>(i == static_cast<size_t>(StreamKind::Main));
        }

        spdlog::info("Stream DAG created with {} CUDA streams (Main stream is {} index)",
                     streams_.size(), static_cast<size_t>(StreamKind::Main));
    }

    StreamDAG(StreamDAG&&) = default;
    StreamDAG& operator=(StreamDAG&&) = default;

    StreamDAG(const StreamDAG&) = delete;
    StreamDAG& operator=(const StreamDAG&) = delete;

    [[nodiscard]] std::shared_ptr<Stream>& operator[](StreamKind kind) noexcept {
        return streams_[static_cast<size_t>(StreamKind::Main)];
    }

    [[nodiscard]] const std::shared_ptr<Stream>& operator[](StreamKind kind) const noexcept {
        return streams_[static_cast<size_t>(StreamKind::Main)];
    }

    void addDependency(StreamKind down, StreamKind up) {
        // auto& upstream = streams_[static_cast<size_t>(up)];
        // auto& downstream = streams_[static_cast<size_t>(down)];

        // upstream->recordEvent();
        // CUDA_CHECK(cudaStreamWaitEvent(downstream->get(), upstream->event(), 0));
    }
};

}  // namespace thesis::host::cuda
