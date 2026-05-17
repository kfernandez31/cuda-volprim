#include "cloud_validation.h"

#include "thesis/host/utils/io.h"
#include "thesis/host/utils/mitsuba_parser.h"
#include "thesis/host/utils/result.h"

#include <spdlog/spdlog.h>

namespace thesis::test::scenes {

using namespace thesis::host::utils;

namespace {

// Shared loader: builds the scene from PLY + Mitsuba config, optionally overriding albedo.
// albedo_override < 0 means "use PLY values."
Result<MultiViewTestScene> build_cloud_scene(std::string name, std::string description,
                                              float sigma_multiplier, float3 albedo_override) {
    MultiViewTestScene scene;
    scene.name = std::move(name);
    scene.description = std::move(description);

    spdlog::info("Loading cloud primitives from PLY...");
    auto primitives_future = thesis::host::utils::io::async::loadPrimitives(
        "assets/cloud/root.primitives_pyr0.ply", sigma_multiplier, albedo_override);
    auto primitives_result = primitives_future.get();

    if (!primitives_result.has_value()) {
        return make_error("Failed to load cloud primitives: {}", primitives_result.error());
    }

    scene.primitives = std::move(primitives_result.value());
    spdlog::info("Loaded {} cloud primitives", scene.primitives.size());

    spdlog::info("Parsing Mitsuba camera configuration...");
    auto config_result = thesis::host::utils::parseMitsubaScene("assets/cloud/__init__.py",
                                                                "assets/cloud/args.json");

    if (!config_result.has_value()) {
        return make_error("Failed to parse Mitsuba config: {}", config_result.error());
    }

    const auto& config = config_result.value();
    spdlog::info("Parsed {} cameras from Mitsuba config", config.cameras.size());

    for (size_t i = 0; i < config.cameras.size(); ++i) {
        const auto& cam_config = config.cameras[i];
        auto camera = thesis::host::utils::createOrthographicCamera(cam_config,
                                                                     config.orthographic_extent);
        scene.cameras.push_back({camera, cam_config.width, cam_config.height});
    }

    spdlog::info("Created {} orthographic camera(s)", scene.cameras.size());

    scene.env_map_override = "assets/white_constant.hdr";
    return scene;
}

}  // namespace

Result<MultiViewTestScene> cloud_asset_validation(float sigma_multiplier) {
    // PLY albedo (≈0): pure absorber. Jorge's optimization used albedo_lr=0, so all
    // albedos stayed at init_albedo=0. Brightness = exp(-τ) × background.
    return build_cloud_scene(
        "cloud_asset_validation",
        "Jorge's cloud asset (pure absorber, PLY albedo ≈ 0) — 652 Gaussians, 24 ortho views",
        sigma_multiplier, make_float3(-1.0f));
}

Result<MultiViewTestScene> cloud_asset_scattering(float sigma_multiplier, float albedo) {
    // Override albedo to make the cloud scattering. Used to validate NEE / MIS / HG /
    // denoiser-AOV code paths that only fire at scatter events.
    return build_cloud_scene(
        "cloud_asset_scattering",
        "Cloud asset with overridden scattering albedo — exercises NEE/MIS/HG paths",
        sigma_multiplier, make_float3(albedo, albedo, albedo));
}

}  // namespace thesis::test::scenes
