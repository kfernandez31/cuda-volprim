#pragma once

#include "thesis/host/params/camera.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/utils/result.h"

#include <optional>
#include <string>
#include <vector>

namespace thesis::test::scenes {

// Multi-view test scene with multiple cameras (for validation against reference renders)
struct CameraView {
    thesis::host::params::Camera camera;
    size_t width;
    size_t height;
};

struct MultiViewTestScene {
    std::string name;
    std::string description;
    std::vector<thesis::device::params::Primitive> primitives;
    std::vector<CameraView> cameras;
    std::optional<std::string> env_map_override;  // Optional constant white env
};

// Load cloud asset and cameras from Mitsuba config
// Returns a multi-view test scene with 652 primitives and 24 orthographic cameras
thesis::host::utils::Result<MultiViewTestScene> cloud_asset_validation(float sigma_multiplier = 60.0f);

// Same primitives + cameras as cloud_asset_validation, but with albedo overridden
// to a high scattering value. Use this to exercise features that only fire at
// scatter events (NEE, MIS, anisotropic phase, denoiser AOVs). The PLY ships
// albedo ≈ 0 (pure absorber), so the validation scene cannot test them.
thesis::host::utils::Result<MultiViewTestScene> cloud_asset_scattering(float sigma_multiplier = 60.0f,
                                                                       float albedo = 0.9f);

// Generic Gaussian-asset scene (bunny etc.) — loads SG_PLY with a single framed perspective
// camera. Used for asset-generalization validation beyond the cloud.
thesis::host::utils::Result<MultiViewTestScene> asset_validation(float sigma_multiplier = 1.0f);

}  // namespace thesis::test::scenes
