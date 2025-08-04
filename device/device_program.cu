// #include "entry/anyhit.cuh"
// #include "entry/closesthit.cuh"
// #include "entry/miss.cuh"
// #include "entry/raygen.cuh"

#ifndef OPTIX_HIT_KIND_SPHERE_FRONT_FACE
#   define OPTIX_HIT_KIND_SPHERE_FRONT_FACE 0xFEu
#   define OPTIX_HIT_KIND_SPHERE_BACK_FACE  0xFFu
#endif

#include "core/launch_params.cuh"
#include "thesis/device/geometry/ray.h"
#include "thesis/device/payloads/closesthit.h"
#include "thesis/device/payloads/miss.h"

#include <optix.h>
#include <vector_types.h>

extern "C" __global__ void __raygen__rg() {
    using namespace thesis::device;
    
    const auto idx = optixGetLaunchIndex();
    const auto dims = optixGetLaunchDimensions();
    
    // Generate ray using the camera
    const auto pixel_idx = make_uint2(idx.x, idx.y);
    const auto jitter = make_float2(0.5f, 0.5f); // Center of pixel, no random jitter
    auto ray = launch_params.camera_.jittered_ray(pixel_idx, jitter);
    
    // Simple trace for debug mode
    unsigned ps[payloads::MAX_PAYLOADS] = {};
    
    optixTrace(
        launch_params.ias_handle_,
        ray.origin_,
        ray.direction_,
        0.0001f,                   // tmin - small epsilon to avoid self-intersection
        1e20f,                     // tmax - very large number
        0.0f,                      // rayTime
        0xFF,                      // visibility mask
        OPTIX_RAY_FLAG_NONE,       // ray flags
        0,                         // SBT offset
        1,                         // SBT stride
        0,                         // miss SBT index
        ps[0], ps[1], ps[2], ps[3]
    );
    
    // Check if we hit or missed
    const auto tag = static_cast<payloads::Tag>(ps[0]);
    float3 color;
    
    if (tag == payloads::Tag::ClosestHit) {
        // Hit - return red
        color = make_float3(1.0f, 0.0f, 0.0f);
        
        // Debug print for center pixel
        if (idx.x == dims.x / 2 && idx.y == dims.y / 2) {
            payloads::ClosestHit hit;
            hit.unpack(ps);
            printf("CENTER HIT: t=%f, prim_idx=%u\n", hit.t_hit, hit.prim_idx);
        }
    } else {
        // Miss - return blue
        color = make_float3(0.0f, 0.0f, 1.0f);
        
        // Debug print for center pixel
        if (idx.x == dims.x / 2 && idx.y == dims.y / 2) {
            printf("CENTER MISS: ray_origin=(%f,%f,%f), ray_dir=(%f,%f,%f)\n",
                   ray.origin_.x, ray.origin_.y, ray.origin_.z,
                   ray.direction_.x, ray.direction_.y, ray.direction_.z);
        }
    }
    
    // Write to image
    const auto global_sample_idx = launch_params.image_.getGlobalSampleIndex(idx);
    launch_params.image_[global_sample_idx] = color;
}

extern "C" __global__ void __miss__ms() {
    using namespace thesis::device;
    
    // For minimal test, just return blue color
    const auto blue = make_float3(0.0f, 0.0f, 1.0f);
    
    payloads::Miss p(blue);
    p.packToOptix();
}

extern "C" __global__ void __closesthit__ch() {
    using namespace thesis::device;
    
    payloads::ClosestHit p;
    p.t_hit    = optixGetRayTmax();
    p.prim_idx = optixGetInstanceId();
    
    const unsigned hitKind = optixGetHitKind();
    p.is_exit = (hitKind == OPTIX_HIT_KIND_SPHERE_BACK_FACE);
    
    // Debug print
    // const auto idx = optixGetLaunchIndex();
    // if (idx.x == optixGetLaunchDimensions().x / 2 && 
    //     idx.y == optixGetLaunchDimensions().y / 2) {
        printf("CH: t=%f, instance=%u, hitKind=%u\n", 
               p.t_hit, p.prim_idx, hitKind);
    // }
    
    p.packToOptix();
}
