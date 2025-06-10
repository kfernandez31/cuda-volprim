#pragma once

#include "thesis/host/cuda/stream.h"
#include "thesis/host/optix/record.h"

#include <cuda.h>
#include <optix_host.h>
#include <optix_stubs.h>
#include <optix_types.h>

#include <memory>

namespace thesis::host::optix {

class SBT {
   private:
    Record<> raygen_record_, miss_record_, hitgroup_record_;
    OptixShaderBindingTable sbt_ = {};

   public:
    SBT() = default;

    SBT(SBT&&) noexcept = default;
    SBT& operator=(SBT&&) noexcept = default;

    SBT(const SBT&) = delete;
    SBT& operator=(const SBT&) = delete;

    SBT(CUcontext ctx, std::shared_ptr<cuda::Stream> stream)
        : raygen_record_(ctx, stream), miss_record_(ctx, stream), hitgroup_record_(ctx, stream) {}

    void build(OptixProgramGroup raygen_pg, OptixProgramGroup miss_pg,
               OptixProgramGroup hitgroup_pg) {
        raygen_record_.build(raygen_pg);
        miss_record_.build(miss_pg);
        hitgroup_record_.build(hitgroup_pg);

        sbt_.raygenRecord = raygen_record_.get();

        sbt_.missRecordBase = miss_record_.get();
        sbt_.missRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
        sbt_.missRecordCount = 1;

        sbt_.hitgroupRecordBase = hitgroup_record_.get();
        sbt_.hitgroupRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
        sbt_.hitgroupRecordCount = 1;
    }

    const OptixShaderBindingTable& get() const noexcept { return sbt_; }
};

}  // namespace thesis::host::optix
