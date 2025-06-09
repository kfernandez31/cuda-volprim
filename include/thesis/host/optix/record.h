#pragma once

#include "thesis/host/cuda/buffer.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <optix_host.h>
#include <optix_stubs.h>
#include <optix_types.h>

#include <array>
#include <cstddef>
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
    cuda::Buffer<SBTRecord<T>> buffer_;

   public:
    Record() = default;

    Record(Record&&) noexcept = default;
    Record& operator=(Record&&) noexcept = default;

    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    Record(OptixProgramGroup pg, const T& data, CUcontext ctx) {
        SBTRecord<T> record = {};
        OPTIX_CHECK(optixSbtRecordPackHeader(pg, &record));
        record.data = data;
        buffer_ = cuda::Buffer<SBTRecord<T>>::onDeviceOnly(&record, 1, ctx);
    }

    [[nodiscard]] CUdeviceptr get() const noexcept {
        return reinterpret_cast<CUdeviceptr>(buffer_.device());
    }
};

template <>
class Record<void> {
   private:
    cuda::Buffer<std::byte> buffer_;

   public:
    Record() = default;

    Record(Record&&) noexcept = default;
    Record& operator=(Record&&) noexcept = default;

    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    Record(OptixProgramGroup pg, CUcontext ctx) {
        alignas(OPTIX_SBT_RECORD_ALIGNMENT) char header[OPTIX_SBT_RECORD_HEADER_SIZE] = {};
        OPTIX_CHECK(optixSbtRecordPackHeader(pg, header));
        buffer_ = cuda::Buffer<std::byte>::onDeviceOnly(reinterpret_cast<std::byte*>(header),
                                                        OPTIX_SBT_RECORD_HEADER_SIZE, ctx);
    }

    [[nodiscard]] CUdeviceptr get() const noexcept {
        return reinterpret_cast<CUdeviceptr>(buffer_.device());
    }
};

}  // namespace thesis::host::optix
