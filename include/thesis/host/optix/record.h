#pragma once

#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <optix_host.h>
#include <optix_stubs.h>
#include <optix_types.h>

#include <array>
#include <cstddef>
#include <memory>
#include <utility>

namespace thesis::host::optix {

template <typename T>
struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) SBTRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
};

template <typename T = void>
class Record {
   private:
    cuda::AsyncBuffer<SBTRecord<T>> buf_;

   public:
    Record() = default;

    Record(Record&&) noexcept = default;
    Record& operator=(Record&&) noexcept = default;

    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    Record(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : buf_(1, ctx, std::move(stream), cuda::AllocType::OnBoth) {}

    void build(OptixProgramGroup pg, const T& data) {
        auto& record = buf_[0];

        OPTIX_CHECK(optixSbtRecordPackHeader(pg, &record.header));
        record.data = data;

        buf_.upload();
    }

    [[nodiscard]] CUdeviceptr get() const noexcept { return buf_; }
};

template <>
class Record<void> {
   private:
    cuda::AsyncBuffer<std::byte> buf_;

   public:
    Record() = default;

    Record(Record&&) noexcept = default;
    Record& operator=(Record&&) noexcept = default;

    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    Record(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : buf_(OPTIX_SBT_RECORD_HEADER_SIZE, ctx, std::move(stream), cuda::AllocType::OnBoth) {}

    void build(OptixProgramGroup pg) {
        OPTIX_CHECK(optixSbtRecordPackHeader(pg, buf_.host()));
        buf_.upload();
    }

    [[nodiscard]] CUdeviceptr get() const noexcept { return buf_.cu_device_ptr(); }
};

}  // namespace thesis::host::optix
