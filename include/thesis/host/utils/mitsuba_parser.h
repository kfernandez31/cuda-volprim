#pragma once

#include "thesis/host/params/camera.h"
#include "thesis/host/utils/result.h"

#include <vector_types.h>

#include <cstddef>
#include <filesystem>
#include <vector>

namespace thesis::host::utils {

// Mitsuba camera configuration
struct MitsubaCameraConfig {
    std::string name;
    float3 origin;
    float3 target;
    float3 up;
    size_t width;
    size_t height;
};

// Mitsuba scene configuration
struct MitsubaSceneConfig {
    std::vector<MitsubaCameraConfig> cameras;
    float orthographic_extent;  // Viewport width for orthographic cameras
    size_t default_width;
    size_t default_height;
};

// Parse Mitsuba __init__.py to extract camera configurations
Result<MitsubaSceneConfig> parseMitsubaScene(const std::filesystem::path& init_py_path,
                                             const std::filesystem::path& args_json_path);

// Create orthographic camera from Mitsuba config
params::Camera createOrthographicCamera(const MitsubaCameraConfig& cam_config,
                                        float orthographic_extent);

}  // namespace thesis::host::utils
