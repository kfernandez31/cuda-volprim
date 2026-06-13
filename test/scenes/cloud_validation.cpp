#include "cloud_validation.h"

#include "thesis/host/utils/io.h"
#include "thesis/host/utils/mitsuba_parser.h"
#include "thesis/host/utils/result.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string_view>

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
        "assets/models/cloud/root.primitives_pyr0.ply", sigma_multiplier, albedo_override);
    auto primitives_result = primitives_future.get();

    if (!primitives_result.has_value()) {
        return make_error("Failed to load cloud primitives: {}", primitives_result.error());
    }

    scene.primitives = std::move(primitives_result.value());
    spdlog::info("Loaded {} cloud primitives", scene.primitives.size());

    spdlog::info("Parsing Mitsuba camera configuration...");
    auto config_result = thesis::host::utils::parseMitsubaScene("assets/models/cloud/__init__.py",
                                                                "assets/models/cloud/args.json");

    if (!config_result.has_value()) {
        return make_error("Failed to parse Mitsuba config: {}", config_result.error());
    }

    const auto& config = config_result.value();
    spdlog::info("Parsed {} cameras from Mitsuba config", config.cameras.size());

    // SG_CAM=<idx> restricts rendering to a single camera (fast iteration / parity
    // with a single-cam Mitsuba reference). Unset = all cameras. The kept camera is
    // emitted as 0000.exr (run_multiview names by vector position).
    const char* sg_cam_env = std::getenv("SG_CAM");
    const int only_cam = sg_cam_env ? std::atoi(sg_cam_env) : -1;
    for (size_t i = 0; i < config.cameras.size(); ++i) {
        if (only_cam >= 0 && static_cast<int>(i) != only_cam) continue;
        const auto& cam_config = config.cameras[i];
        auto camera = thesis::host::utils::createOrthographicCamera(cam_config,
                                                                     config.orthographic_extent);
        scene.cameras.push_back({camera, cam_config.width, cam_config.height});
    }
    if (only_cam >= 0) {
        spdlog::info("SG_CAM={}: rendering single camera only", only_cam);
    }

    spdlog::info("Created {} orthographic camera(s)", scene.cameras.size());

    // SG_ENV=meadow swaps the constant env for the real 4k HDR (orientation
    // calibrated in FINDINGS §8.6; mirrored on the Mitsuba side via
    // render_cloud_prb_absorption.py). Default keeps the constant env.
    const char* sg_env = std::getenv("SG_ENV");
    const std::string_view env_sel = sg_env ? std::string_view(sg_env) : std::string_view{};
    scene.env_map_override = env_sel == "meadow"
                                 ? "assets/environment_maps/meadow_2_4k.hdr"
                             : env_sel == "studio"
                                 ? "assets/environment_maps/ferndale_studio_01_4k.hdr"
                                 : "assets/environment_maps/white_constant.hdr";
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

// Generic Gaussian-asset loader (bunny etc.). SG_PLY = path (default bunny pyr0); a single
// framed perspective camera (SG_DIST / SG_FOV / SG_RES); SG_ALBEDO override; SG_ENV env.
// Assets are centered in ~[-1,1]³ so a perspective cam at ~3.5 looking at origin frames them.
Result<MultiViewTestScene> asset_validation(float sigma_multiplier) {
    auto envf = [](const char* k, double d) {
        const char* v = std::getenv(k); return v ? std::atof(v) : d;
    };
    const char* ply_env = std::getenv("SG_PLY");
    const std::string ply = ply_env ? ply_env : "assets/models/bunny/bunny_pyr0.ply";
    const float alb = static_cast<float>(envf("SG_ALBEDO", 0.0));
    const float3 albedo_override = alb > 0.0f ? make_float3(alb, alb, alb) : make_float3(-1.0f);

    MultiViewTestScene scene;
    scene.name = "asset_validation";
    scene.description = "Generic Gaussian asset: " + ply;

    spdlog::info("Loading asset PLY: {}", ply);
    auto fut = thesis::host::utils::io::async::loadPrimitives(ply, sigma_multiplier, albedo_override);
    auto res = fut.get();
    if (!res.has_value()) return make_error("Failed to load asset '{}': {}", ply, res.error());
    scene.primitives = std::move(res.value());
    spdlog::info("Loaded {} asset primitives", scene.primitives.size());

    const auto dist = static_cast<float>(envf("SG_DIST", 3.5));
    const auto fov = static_cast<float>(envf("SG_FOV", 40.0));
    const auto res_ = static_cast<size_t>(envf("SG_RES", 512));
    const char* view_env = std::getenv("SG_VIEW");
    const std::string view = view_env ? view_env : "negz";
    float3 origin, up = make_float3(0.0f, 1.0f, 0.0f);
    if (view == "posz") origin = make_float3(0, 0, dist);
    else if (view == "posx") origin = make_float3(dist, 0, 0);
    else if (view == "negx") origin = make_float3(-dist, 0, 0);
    else if (view == "posy") { origin = make_float3(0, dist, 0); up = make_float3(0, 0, 1); }
    else if (view == "negy") { origin = make_float3(0, -dist, 0); up = make_float3(0, 0, 1); }
    else if (view == "diag") origin = make_float3(dist * 0.6f, dist * 0.5f, -dist * 0.6f);
    else origin = make_float3(0, 0, -dist);  // negz default
    auto camera = thesis::host::params::Camera::createPerspective(
        res_, res_, origin, make_float3(0.0f, 0.0f, 0.0f), up, fov);
    scene.cameras.push_back({camera, res_, res_});

    const char* sg_env = std::getenv("SG_ENV");
    const std::string_view env_sel = sg_env ? std::string_view(sg_env) : std::string_view{};
    scene.env_map_override = env_sel == "meadow"
                                 ? "assets/environment_maps/meadow_2_4k.hdr"
                             : env_sel == "studio"
                                 ? "assets/environment_maps/ferndale_studio_01_4k.hdr"
                                 : "assets/environment_maps/white_constant.hdr";
    return scene;
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
