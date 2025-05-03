#include <optix.h>
// #include <optix_device.h> // TODO: which one?

#include "sutil/vec_math.h"
#include "thesis/optix/launch_params.h"

// TODO: is this good enough?
#define VISIBILITY_ALL 0xFF
#define INF_F 1e20f

namespace thesis {
namespace optix {

extern "C" __constant__ LaunchParams params;

static __forceinline__ __device__ void setPayload(const float3& p) {
    optixSetPayload_0(__float_as_uint(p.x));
    optixSetPayload_1(__float_as_uint(p.y));
    optixSetPayload_2(__float_as_uint(p.z));
}

static __forceinline__ __device__ void computeRay(const uint3& idx, const uint3& dim,
                                                  float3& origin, float3& direction) {
    const auto screen_uv = make_float2(static_cast<float>(idx.x), static_cast<float>(idx.y));
    origin = params.camera_.eye_;
    direction = normalize(params.camera_.pixel00_ + screen_uv.x * params.camera_.pixel_du_ +
                          screen_uv.y * params.camera_.pixel_dv_ - origin);
}

// TODO: replace with Sobol at some point
static __forceinline__ __device__ float2 sample_random_2d(const uint3& idx, int sample_index) {
    // Simple hash function combining pixel index and sample index
    auto seed = idx.x * 73856093u ^ idx.y * 19349663u ^ sample_index * 83492791u;

    // Xorshift32 RNG
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    // Convert to float in [-0.5, 0.5]
    auto x = (static_cast<float>((seed >> 0) & 0xFFFFu) / 65536.0f) - 0.5f;
    auto y = (static_cast<float>((seed >> 16) & 0xFFFFu) / 65536.0f) - 0.5f;

    return make_float2(x, y);
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

extern "C" __global__ void __raygen__rg() {
    const auto idx = optixGetLaunchIndex();
    const auto dim = optixGetLaunchDimensions();

    const auto pixel = make_float2(idx.x, idx.y);
    auto color = make_float3(0.0f);

    for (int s = 0; s < params.num_samples_per_pixel_; ++s) {
        const auto jitter =
            sample_random_2d(idx, s);  // Must be deterministic and unique per (idx, s)

        auto ray_origin = params.camera_.eye_;
        auto ray_direction =
            normalize(params.camera_.pixel00_ + (pixel.x + jitter.x) * params.camera_.pixel_du_ +
                      (pixel.y + jitter.y) * params.camera_.pixel_dv_ - ray_origin);

        color += params.env_map_.sample(ray_direction);
    }

    color /= static_cast<float>(params.num_samples_per_pixel_);
    params.image_(idx.x, idx.y) = color;
}

extern "C" __global__ void __miss__ms() {
    const auto ray_direction = optixGetWorldRayDirection();
    const auto color = params.env_map_.sample(ray_direction);
    setPayload(color);
}

// TODO: should this be like this?
extern "C" __global__ void __closesthit__ch() {
    // When built-in triangle intersection is used, a number of fundamental
    // attributes are provided by the OptiX API, indlucing barycentric coordinates.
    const float2 barycentrics = optixGetTriangleBarycentrics();

    setPayload(make_float3(barycentrics, 1.0f));
}

}  // namespace optix
}  // namespace thesis
