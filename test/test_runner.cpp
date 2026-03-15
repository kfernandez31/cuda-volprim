#include "thesis/pch.h"

#include "scenes/cloud_validation.h"
#include "scenes/geometric_validation.h"
#include "thesis/host/app/config.h"
#include "thesis/host/app/logging.h"
#include "thesis/host/app/renderer.h"
#include "thesis/host/utils/result.h"

#ifndef OPTIX_FUNCTION_TABLE_INCLUDED
#define OPTIX_FUNCTION_TABLE_INCLUDED
#include <optix_function_table_definition.h>
#endif  // OPTIX_FUNCTION_TABLE_INCLUDED

#include <CLI11/CLI11.hpp>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace thesis::host;
using namespace thesis::test::scenes;

struct TestConfig {
    std::string scene_name;
    bool list_scenes = false;
    bool run_all = false;
    bool run_validation = false;
    bool run_stress = false;
    size_t spp = 64;
    size_t width = 1920;
    size_t height = 1080;
    std::string output_dir = "test_results";
    std::string output_file = "";
    float sigma_multiplier = 7.5f;  // Default sigma_t scaling factor
};

void list_scenes(const std::string& category = "all") {
    std::vector<TestScene> scenes;
    std::string header;

    if (category == "validation") {
        scenes = get_validation_test_scenes();
        header = "Validation Test Scenes";
    } else if (category == "stress") {
        scenes = get_stress_test_scenes();
        header = "Performance Stress Test Scenes";
    } else {
        scenes = get_all_test_scenes();
        header = "All Test Scenes";
    }

    std::cout << "\n" << header << " (" << scenes.size() << " total):\n\n";

    for (const auto& scene : scenes) {
        std::cout << "  " << scene.name << "\n";
        std::cout << "    " << scene.description << "\n";
    }
    std::cout << "\n";
}

utils::Result<TestConfig> parse_args(int argc, char* argv[]) {
    TestConfig config;

    CLI::App app{"Test Runner for Gaussian Volumetric Path Tracer"};

    // Test execution modes
    app.add_option("--scene", config.scene_name, "Run specific test scene");
    app.add_flag("--list", config.list_scenes, "List all available test scenes");
    app.add_flag("--all", config.run_all, "Run all test scenes");
    app.add_flag("--validation", config.run_validation, "Run validation/correctness tests only");
    app.add_flag("--stress", config.run_stress, "Run performance stress tests only");

    // Render settings
    app.add_option("--spp", config.spp, "Samples per pixel")->default_val(64)->check(CLI::PositiveNumber);
    app.add_option("--sigma-multiplier", config.sigma_multiplier, "Sigma_t scaling factor")->default_val(7.5f)->check(CLI::PositiveNumber);
    app.add_option("--width", config.width, "Image width")->default_val(1920)->check(CLI::PositiveNumber);
    app.add_option("--height", config.height, "Image height")->default_val(1080)->check(CLI::PositiveNumber);

    // Output settings
    app.add_option("--output", config.output_file, "Output file (for single scene)");
    app.add_option("--output-dir", config.output_dir, "Output directory (for --all)")->default_val("test_results");

    argv = app.ensure_utf8(argv);
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::ostringstream oss;
        auto code = app.exit(e, oss);

        // Code 0 means help/version (not an error) - print and exit
        if (code == 0) {
            std::cout << oss.str();
            std::exit(0);
        }

        // Actual error
        return utils::make_error("Argument parse error (code {}): {}", code, oss.str());
    }

    return config;
}

void run_test_scene(const TestScene& scene, const TestConfig& test_config, const std::string& output_path) {
    std::cout << "\n────────────────────────────────────────────────────────\n";
    std::cout << "Running test: " << scene.name << "\n";
    std::cout << "Description: " << scene.description << "\n";
    std::cout << "Primitives: " << scene.primitives.size() << "\n";
    std::cout << "SPP: " << test_config.spp << "\n";
    std::cout << "Resolution: " << test_config.width << "×" << test_config.height << "\n";
    std::cout << "Output: " << output_path << "\n";
    std::cout << "────────────────────────────────────────────────────────\n";

    // Create config for renderer
    app::Config renderer_config;
    renderer_config.image_width_ = test_config.width;
    renderer_config.image_height_ = test_config.height;
    renderer_config.num_samples_per_pixel_ = test_config.spp;
    renderer_config.output_path_ = output_path;
    renderer_config.seed_ = 42;  // Fixed seed for reproducibility

    // Assume env map and module paths are in default locations
    renderer_config.env_map_path_ = "assets/meadow_2_4k.hdr";
    renderer_config.module_blob_path_ = "build/device_program.optixir";

    try {
        // Create renderer with test scene primitives
        app::Renderer renderer(renderer_config, std::vector<params::Primitive>(scene.primitives));

        // Render
        renderer.render();

        std::cout << "✓ Test completed successfully\n";
    } catch (const std::exception& e) {
        std::cerr << "✗ Test failed with exception: " << e.what() << "\n";
        throw;
    }
}

void run_multiview_test(const MultiViewTestScene& scene, const TestConfig& test_config) {
    std::cout << "\n════════════════════════════════════════════════════════\n";
    std::cout << "Running multi-view test: " << scene.name << "\n";
    std::cout << "Description: " << scene.description << "\n";
    std::cout << "Primitives: " << scene.primitives.size() << "\n";
    std::cout << "Cameras: " << scene.cameras.size() << "\n";
    std::cout << "SPP: " << test_config.spp << "\n";
    std::cout << "════════════════════════════════════════════════════════\n";

    // Validate scene has content
    if (scene.cameras.empty()) {
        std::cerr << "✗ Multi-view test failed: No cameras available\n";
        std::cerr << "  This likely means Mitsuba config parsing failed.\n";
        std::cerr << "  Check logs for errors related to __init__.py or args.json parsing.\n";
        throw std::runtime_error("No cameras to render");
    }

    if (scene.primitives.empty()) {
        std::cerr << "✗ Multi-view test failed: No primitives loaded\n";
        std::cerr << "  Check that assets/cloud/root.primitives.ply exists and is valid.\n";
        throw std::runtime_error("No primitives to render");
    }

    // Create output directory for this test
    fs::path output_dir = fs::path(test_config.output_dir) / scene.name;
    try {
        fs::create_directories(output_dir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "✗ Failed to create output directory: " << e.what() << "\n";
        throw;
    }

    // Render each camera view
    for (size_t cam_idx = 0; cam_idx < scene.cameras.size(); ++cam_idx) {
        const auto& [camera, cam_width, cam_height] = scene.cameras[cam_idx];

        // Generate output path: test_results/cloud_asset_validation/0000.exr
        std::ostringstream oss;
        oss << std::setw(4) << std::setfill('0') << cam_idx << ".exr";
        fs::path output_path = output_dir / oss.str();

        std::cout << "\n  [" << (cam_idx + 1) << "/" << scene.cameras.size() << "] ";
        std::cout << "Rendering camera " << cam_idx << " → " << output_path.string() << "\n";
        std::cout << "    Resolution: " << cam_width << "×" << cam_height;
        std::cout << " (" << (camera.is_orthographic() ? "orthographic" : "perspective") << ")\n";

        // Create renderer config
        app::Config renderer_config;
        renderer_config.image_width_ = cam_width;
        renderer_config.image_height_ = cam_height;
        renderer_config.num_samples_per_pixel_ = test_config.spp;
        renderer_config.output_path_ = output_path.string();
        renderer_config.seed_ = 42;

        // Use override env map if specified, otherwise default
        renderer_config.env_map_path_ =
            scene.env_map_override.value_or("assets/meadow_2_4k.hdr");
        renderer_config.module_blob_path_ = "build/device_program.optixir";

        try {
            // Create renderer with test scene primitives and specific camera
            app::Renderer renderer(renderer_config,
                                   std::vector<params::Primitive>(scene.primitives), camera);

            // Render
            renderer.render();

            std::cout << "    ✓ Camera " << cam_idx << " completed\n";
        } catch (const std::exception& e) {
            std::cerr << "    ✗ Camera " << cam_idx << " failed: " << e.what() << "\n";
            throw;
        }
    }

    std::cout << "\n✓ Multi-view test completed successfully\n";
    std::cout << "  Output directory: " << output_dir.string() << "\n";
}

int main(int argc, char* argv[]) {
    app::logging::init();

    // Validate working directory (test runner must be run from project root)
    if (!fs::exists("assets") || !fs::exists("build")) {
        std::cerr << "Error: Test runner must be executed from project root directory.\n";
        std::cerr << "Current directory: " << fs::current_path() << "\n";
        std::cerr << "Expected assets/ and build/ directories to exist.\n";
        return 1;
    }

    auto test_config = utils::try_unwrap_or_exit(parse_args(argc, argv));

    // Handle --list
    if (test_config.list_scenes) {
        if (test_config.run_validation) {
            list_scenes("validation");
        } else if (test_config.run_stress) {
            list_scenes("stress");
        } else {
            list_scenes("all");
        }
        return 0;
    }

    // Determine which scenes to run
    std::vector<TestScene> scenes_to_run;
    std::string run_mode;

    if (test_config.run_validation) {
        scenes_to_run = get_validation_test_scenes();
        run_mode = "validation";
    } else if (test_config.run_stress) {
        scenes_to_run = get_stress_test_scenes();
        run_mode = "stress";
    } else if (test_config.run_all) {
        scenes_to_run = get_all_test_scenes();
        run_mode = "all";
    } else {
        // Default: all scenes for --all or single scene mode
        scenes_to_run = get_all_test_scenes();
    }

    // Handle --validation, --stress, or --all (run multiple scenes)
    if (test_config.run_validation || test_config.run_stress || test_config.run_all) {
        // Create output directory
        fs::create_directories(test_config.output_dir);

        std::cout << "Running " << run_mode << " tests (" << scenes_to_run.size() << " scenes)...\n";

        size_t passed = 0;
        size_t failed = 0;

        for (const auto& scene : scenes_to_run) {
            std::string output_path = test_config.output_dir + "/" + scene.name + ".exr";

            try {
                run_test_scene(scene, test_config, output_path);
                ++passed;
            } catch (...) {
                ++failed;
                std::cerr << "Continuing with next test...\n";
            }
        }

        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "Test Summary:\n";
        std::cout << "  Mode:   " << run_mode << "\n";
        std::cout << "  Total:  " << scenes_to_run.size() << "\n";
        std::cout << "  Passed: " << passed << "\n";
        std::cout << "  Failed: " << failed << "\n";
        std::cout << "════════════════════════════════════════════════════════\n";

        return (failed > 0) ? 1 : 0;
    }

    // Handle single scene
    if (!test_config.scene_name.empty()) {
        // Special case: cloud_asset_validation (multi-view test)
        if (test_config.scene_name == "cloud_asset_validation") {
            try {
                auto cloud_scene_result = cloud_asset_validation(test_config.sigma_multiplier);
                if (!cloud_scene_result.has_value()) {
                    std::cerr << "✗ Failed to load cloud asset: " << cloud_scene_result.error().msg_
                              << "\n";
                    return 1;
                }
                run_multiview_test(cloud_scene_result.value(), test_config);
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "✗ Exception: " << e.what() << "\n";
                return 1;
            } catch (...) {
                std::cerr << "✗ Unknown exception occurred\n";
                return 1;
            }
        }

        // Find the requested scene
        auto it = std::find_if(scenes_to_run.begin(), scenes_to_run.end(),
            [&](const TestScene& s) { return s.name == test_config.scene_name; });

        if (it == scenes_to_run.end()) {
            std::cerr << "Error: Scene '" << test_config.scene_name << "' not found.\n";
            std::cerr << "Use --list to see available scenes.\n";
            return 1;
        }

        // Determine output path
        std::string output_path = test_config.output_file.empty()
            ? (test_config.scene_name + ".exr")
            : test_config.output_file;

        try {
            run_test_scene(*it, test_config, output_path);
            return 0;
        } catch (...) {
            return 1;
        }
    }

    // No action specified
    std::cerr << "Error: No action specified. Use --scene, --all, or --list.\n";
    std::cerr << "Run with --help for usage information.\n";
    return 1;
}
