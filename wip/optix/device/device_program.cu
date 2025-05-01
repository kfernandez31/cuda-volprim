#include <optix.h>
// #include <optix_device.h> // TODO: which one?

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
    const auto screen_uv = make_float2(static_cast<float>(idx.x), static_cast<float>(idx.y));
    origin    = params.camera.eye;
    direction = normalize(params.camera.pixel00 +
                          screen_uv.x * params.camera.du +
                          screen_uv.y * params.camera.dv - origin);
}

// TODO: use this instead 
/*
static __forceinline__ __device__ void computeRay(const uint3& idx, const uint3& dim, float3& origin, float3& direction)
{
    // Convert launch index to screen-space UV coordinates
    const float2 pixel_idx = make_float2(float(idx.x), float(idx.y));

    // TODO: Replace with an actual RNG per pixel.
    // For example:
    // - Use a hash-based PRNG like TEA or PCG seeded with launch index.
    // - Store per-pixel RNG state in a buffer and advance it.
    // Example stub:
    float2 jitter = make_float2(rng_float(), rng_float()); // rng_float() ∈ [0, 1)
    jitter -= 0.5f; // Center jitter in [-0.5, 0.5]

    const float2 pixel_sample = pixel_idx + jitter;

    origin = params.camera.eye;
    direction = normalize(
        params.camera.pixel00 +
        pixel_sample.x * params.camera.du +
        pixel_sample.y * params.camera.dv -
        origin
    );
}
*/

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
