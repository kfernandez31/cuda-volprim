#pragma once

#include "thesis/host/optix/acceleration_structure.h"

namespace thesis::host::optix {

inline constexpr uint IAS_BUILD_FLAGS = 
    OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;

class IAS : public AccelerationStructure {
   public:
    using AccelerationStructure::AccelerationStructure;

    void build(const OptixBuildInput& input, CUcontext cuda_ctx, OptixDeviceContext optix_ctx) {
        build_internal(input, cuda_ctx, optix_ctx, IAS_BUILD_FLAGS, "IAS");
    }
};

}  // namespace thesis::host::optix