#pragma once

#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"

#include <cuda.h>
#include <optix_stubs.h>

#include <memory>

namespace thesis::host::optix {

class Record {
   private:
    cuda::AsyncBuffer<std::byte> buf_;

   public:
    Record(Record&&) noexcept = default;
    Record& operator=(Record&&) noexcept = default;

    Record(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : buf_(OPTIX_SBT_RECORD_HEADER_SIZE, ctx, std::move(stream), cuda::AllocType::OnBoth) {}

    void build(OptixProgramGroup pg) {
        OPTIX_CHECK(optixSbtRecordPackHeader(pg, buf_.host()));
        buf_.upload();
    }

    [[nodiscard]] CUdeviceptr get() const noexcept { return buf_.cu_device_ptr(); }
};

}  // namespace thesis::host::optix
