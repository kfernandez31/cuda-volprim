#include "cloud_validation.h"

#include "thesis/host/utils/io.h"
#include "thesis/host/utils/mitsuba_parser.h"
#include "thesis/host/utils/result.h"

#include <spdlog/spdlog.h>

namespace thesis::test::scenes {

using namespace thesis::host::utils;

Result<MultiViewTestScene> cloud_asset_validation(float sigma_multiplier) {
    MultiViewTestScene scene;
    scene.name = "cloud_asset_validation";
    scene.description =
        "Jorge's cloud asset with 652 Gaussian primitives and 24 orthographic camera views";

    // Load primitives from PLY
    spdlog::info("Loading cloud primitives from PLY...");
    auto primitives_future =
        thesis::host::utils::io::async::loadPrimitives("assets/cloud/root.primitives_pyr0.ply", sigma_multiplier);
    auto primitives_result = primitives_future.get();

    if (!primitives_result.has_value()) {
        return make_error("Failed to load cloud primitives: {}", primitives_result.error());
    }

    scene.primitives = std::move(primitives_result.value());
    spdlog::info("Loaded {} cloud primitives", scene.primitives.size());

    // Parse Mitsuba configuration
    spdlog::info("Parsing Mitsuba camera configuration...");
    auto config_result = thesis::host::utils::parseMitsubaScene("assets/cloud/__init__.py",
                                                                "assets/cloud/args.json");

    if (!config_result.has_value()) {
        return make_error("Failed to parse Mitsuba config: {}", config_result.error());
    }

    const auto& config = config_result.value();
    spdlog::info("Parsed {} cameras from Mitsuba config", config.cameras.size());

    // Create orthographic cameras
    scene.cameras.reserve(config.cameras.size());
    for (const auto& cam_config : config.cameras) {
        auto camera = thesis::host::utils::createOrthographicCamera(cam_config,
                                                                     config.orthographic_extent);
        scene.cameras.push_back({camera, cam_config.width, cam_config.height});
    }

    spdlog::info("Created {} orthographic cameras", scene.cameras.size());

    // Use white constant environment to match Mitsuba reference
    scene.env_map_override = "assets/white_constant.hdr";

    return scene;
}

}  // namespace thesis::test::scenes
