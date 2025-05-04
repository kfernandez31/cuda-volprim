#include <optix.h>

#include "thesis/optix/launch_params.h"

#include "common.cuh"

extern "C" __global__ void __closesthit__ch() {
    const auto barycentrics = optixGetTriangleBarycentrics();
    setPayload(make_float3(barycentrics, 1.0f));
}