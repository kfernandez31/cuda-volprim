// minimal_sphere_device.cu
#include <optix.h>
#include <cuda_runtime.h>

struct LaunchParams {
    float3* image;
    unsigned int width;
    unsigned int height;
    OptixTraversableHandle handle;
};

extern "C" {
__constant__ LaunchParams params;
}

extern "C" __global__ void __raygen__rg() {
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();
    
    // Generate ray from camera at (0,0,-5) looking at origin
    float3 origin = make_float3(0.0f, 0.0f, -5.0f);
    float3 direction;
    direction.x = (idx.x / (float)dim.x - 0.5f) * 2.0f;
    direction.y = (idx.y / (float)dim.y - 0.5f) * 2.0f;
    direction.z = 1.0f;
    
    // Normalize direction
    float len = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    direction.x /= len;
    direction.y /= len;
    direction.z /= len;
    
    // Trace
    unsigned int hit = 0;
    optixTrace(params.handle,
               origin,
               direction,
               0.0f,    // tmin
               1e20f,   // tmax
               0.0f,    // rayTime
               255,     // visibilityMask
               OPTIX_RAY_FLAG_NONE,
               0,       // SBT offset
               1,       // SBT stride
               0,       // missSBTIndex
               hit);
    
    // Write result
    const unsigned int index = idx.y * params.width + idx.x;
    if (hit) {
        params.image[index] = make_float3(1.0f, 0.0f, 0.0f); // Red for hit
    } else {
        params.image[index] = make_float3(0.2f, 0.2f, 0.2f); // Gray for miss
    }
}

extern "C" __global__ void __miss__ms() {
    optixSetPayload_0(0);
}

extern "C" __global__ void __closesthit__ch() {
    // Debug print
    const uint3 idx = optixGetLaunchIndex();
    if (idx.x == 256 && idx.y == 256) {  // Center pixel
        printf("HIT SPHERE at t=%f\n", optixGetRayTmax());
    }
    optixSetPayload_0(1);
}