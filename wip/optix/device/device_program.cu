#include <optix.h>
// #include <optix_device.h>

#include "thesis/optix/launch_params.h"
#include "sutil/vec_math.h"

// TODO: is this good enough?
#define VISIBILITY_ALL 0xFF
#define INF_F 1e20f

namespace thesis {
namespace optix {

extern "C" __constant__ LaunchParams params;
    
static __forceinline__ __device__ void setPayload(const float3& p)
{
    optixSetPayload_0(__float_as_uint( p.x ));
    optixSetPayload_1(__float_as_uint( p.y ));
    optixSetPayload_2(__float_as_uint( p.z ));
}

static __forceinline__ __device__ void computeRay(const uint3& idx, const uint3& dim, float3& origin, float3& direction)
{
    const auto d = 2.0f * make_float2(float(idx.x), float(idx.y)) / make_float2(float(dim.x), float(dim.y)) - 1.0f; // TODO: nicer casts
    origin    = params.cam_eye;
    direction = normalize(d.x * params.cam_u + d.y * params.cam_v + params.cam_w);
}

/*
extern "C" __global__ void __raygen__rg()
{
    // Lookup our location within the launch grid
    const auto idx = optixGetLaunchIndex();
    const auto dim = optixGetLaunchDimensions();

    // Map our launch idx to a screen location and create a ray from the camera
    // location through the screen
    float3 ray_origin, ray_direction;
    computeRay(idx, dim, ray_origin, ray_direction);

    // Trace the ray against our scene hierarchy
    uint3 p;
    optixTrace(
        params.handle,
        ray_origin,
        ray_direction,
        0.0f,                                // Min intersection distance
        INF_F,                               // Max intersection distance
        0.0f,                                // Disable motion blur
        OptixVisibilityMask(VISIBILITY_ALL),
        OPTIX_RAY_FLAG_NONE,
        0,                                   // 0 - radiance, 1 - shadow, 2 - reflection
        RAY_TYPE_COUNT,
        0,                                   // Use first miss program
        p.x, p.y, p.z
    );

    auto result = make_float3(__uint_as_float(p.x), __uint_as_float(p.y), __uint_as_float(p.z));

    // Record results in our output raster
    params.image[idx.y * params.image_width + idx.x] = result;
}
*/

// TODO: replace with above once this works
extern "C" __global__ void __raygen__rg()
{
    const auto idx = optixGetLaunchIndex();
    const auto dim = optixGetLaunchDimensions();

    float3 ray_origin, ray_direction;
    computeRay(idx, dim, ray_origin, ray_direction);

    // Skip optixTrace — force a miss by sampling environment map directly
    const auto color = params.env_map.sample(ray_direction);

    // Write result directly
    params.image[idx.y * params.image_width + idx.x] = color;
}

extern "C" __global__ void __miss__ms()
{
    const auto ray_direction = optixGetWorldRayDirection();
    const auto color = params.env_map.sample(ray_direction);
    setPayload(color);
}

// TODO: should this be like this?
extern "C" __global__ void __closesthit__ch()
{
    // When built-in triangle intersection is used, a number of fundamental
    // attributes are provided by the OptiX API, indlucing barycentric coordinates.
    const float2 barycentrics = optixGetTriangleBarycentrics();

    setPayload(make_float3( barycentrics, 1.0f ));
}

} // namespace optix 
} // namespace thesis
