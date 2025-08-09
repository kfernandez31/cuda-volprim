#pragma once

#include "core/launch_params.cuh"
#include "thesis/device/payloads/miss.h"

#include <optix.h>
#include <vector_types.h>

extern "C" __global__ void __miss__ms() {
    using namespace thesis::device;
    
    auto color = make_float3(0.0f, 0.0f, 1.0f);
    payloads::Miss p(color);
    p.packToOptix();
}
