#pragma once

#include <optix.h>
#include <vector_types.h>

#include "thesis/device/environment_map.h"
#include "thesis/device/camera.h"

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
    OptixTraversableHandle handle;
    float3* image; // TODO: wrap in class
    size_t image_width;
    size_t image_height;
    device::EnvironmentMap env_map;
    device::Camera camera;
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
