#pragma once

#include <optix.h>
#include <vector_types.h>

#include "thesis/environment_map.h"

#include <cstddef>

#ifdef __cplusplus
namespace thesis {
namespace optix {
#endif // __cplusplus

enum RayType
{
    RAY_TYPE_RADIANCE = 0,
    RAY_TYPE_COUNT,
};

__align__(16) struct LaunchParams 
{
    float3* image;
    size_t image_width;
    size_t image_height;
    float3 cam_eye;
    float3 cam_u, cam_v, cam_w;
    OptixTraversableHandle handle;
    DeviceEnvironmentMap env_map;
};

struct RayGenData
{
    // No data needed
};

struct MissData
{
    // No data needed
};

struct HitGroupData
{
    // No data needed
};

#ifdef __cplusplus
}  // namespace optix
}  // namespace thesis
#endif // __cplusplus
