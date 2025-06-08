#pragma once

#include "thesis/host/optix/record.h"

#include <cuda.h>
#include <optix_host.h>
#include <optix_stubs.h>
#include <optix_types.h>

namespace thesis::host::optix {

class SBTHandle {
   private:
    Record<void> raygen_record_;
    Record<void> miss_record_;
    Record<void> hitgroup_record_;
    OptixShaderBindingTable sbt_ = {};

   public:
    SBTHandle() = default;

    SBTHandle(SBTHandle&& other) noexcept = default;
    SBTHandle& operator=(SBTHandle&& other) noexcept = default;

    SBTHandle(const SBTHandle&) = delete;
    SBTHandle& operator=(const SBTHandle&) = delete;

    SBTHandle(OptixProgramGroup raygen, OptixProgramGroup miss, OptixProgramGroup hitgroup)
        : raygen_record_(raygen), miss_record_(miss), hitgroup_record_(hitgroup) {
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
