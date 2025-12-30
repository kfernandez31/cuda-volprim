#include "thesis/pch.h"

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
    size_t spp = 64;
    size_t width = 1920;
    size_t height = 1080;
    std::string output_dir = "test_results";
    std::string output_file = "";
};

void list_scenes() {
    auto scenes = get_all_test_scenes();
    std::cout << "\nAvailable Test Scenes (" << scenes.size() << " total):\n\n";

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

    // Render settings
    app.add_option("--spp", config.spp, "Samples per pixel")->default_val(64)->check(CLI::PositiveNumber);
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
    std::cout << "Debug: ENABLED (diagnostic output will be shown)\n";
    std::cout << "────────────────────────────────────────────────────────\n";

    // Create config for renderer
    app::Config renderer_config;
    renderer_config.image_width_ = test_config.width;
    renderer_config.image_height_ = test_config.height;
    renderer_config.num_samples_per_pixel_ = test_config.spp;
    renderer_config.output_path_ = output_path;
    renderer_config.seed_ = 42;  // Fixed seed for reproducibility
    renderer_config.debug_ = true;  // Always enable debug for test runner to diagnose issues

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

int main(int argc, char* argv[]) {
    app::logging::init();

    auto test_config = utils::try_unwrap_or_exit(parse_args(argc, argv));

    // Handle --list
    if (test_config.list_scenes) {
        list_scenes();
        return 0;
    }

    // Get all scenes
    auto all_scenes = get_all_test_scenes();

    // Handle --all
    if (test_config.run_all) {
        // Create output directory
        fs::create_directories(test_config.output_dir);

        std::cout << "Running all " << all_scenes.size() << " test scenes...\n";

        size_t passed = 0;
        size_t failed = 0;

        for (const auto& scene : all_scenes) {
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
        std::cout << "  Total:  " << all_scenes.size() << "\n";
        std::cout << "  Passed: " << passed << "\n";
        std::cout << "  Failed: " << failed << "\n";
        std::cout << "════════════════════════════════════════════════════════\n";

        return (failed > 0) ? 1 : 0;
    }

    // Handle single scene
    if (!test_config.scene_name.empty()) {
        // Find the requested scene
        auto it = std::find_if(all_scenes.begin(), all_scenes.end(),
            [&](const TestScene& s) { return s.name == test_config.scene_name; });

        if (it == all_scenes.end()) {
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
