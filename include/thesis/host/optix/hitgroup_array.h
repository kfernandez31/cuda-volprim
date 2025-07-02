// thesis/host/optix/hitgroup_array.h
#pragma once
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"

#include <optix_stubs.h>

#include <memory>

namespace thesis::host::optix {

class HitgroupArray {
    cuda::AsyncBuffer<std::byte> buf_;
    size_t count_ = 0;

   public:
    HitgroupArray(CUcontext ctx, std::shared_ptr<cuda::Stream> stream, size_t cnt)
        : buf_(cnt * OPTIX_SBT_RECORD_HEADER_SIZE, ctx, std::move(stream), cuda::AllocType::OnBoth),
          count_(cnt) {}

    HitgroupArray() = default;

    HitgroupArray(HitgroupArray&&) noexcept = default;
    HitgroupArray& operator=(HitgroupArray&&) noexcept = default;

    HitgroupArray(const HitgroupArray&) = delete;
    HitgroupArray& operator=(const HitgroupArray&) = delete;

    void build(OptixProgramGroup pg) {
        for (size_t i = 0; i < count_; ++i) {
            auto* dst = &buf_[i * OPTIX_SBT_RECORD_HEADER_SIZE];
            OPTIX_CHECK(optixSbtRecordPackHeader(pg, dst));
        }
        buf_.upload();
    }

    [[nodiscard]] CUdeviceptr base() const { return buf_.cu_device_ptr(); }
    [[nodiscard]] size_t count() const { return count_; }
};

}  // namespace thesis::host::optix
