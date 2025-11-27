#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/utils/result.h"

#include <CLI11/CLI11.hpp>
#include <cstddef>
#include <filesystem>
#include <sstream>
#include <string>

namespace thesis::host::app {

namespace fs = std::filesystem;

struct Config {
    std::string raygen_function_name_ = "__raygen__rg";
    std::string miss_function_name_ = "__miss__ms";
    std::string anyhit_function_name_ = "__anyhit__ah";
    std::string launch_params_variable_name_ = "launch_params";

    fs::path output_path_ = "output.exr";
    fs::path module_blob_path_ = fs::path("build") / "device_program.optixir";
    fs::path env_map_path_ = fs::path("assets") / "meadow_2_4k.hdr";

    size_t num_samples_per_pixel_ = 4;
    size_t image_width_ = 1000;
    size_t image_height_ = 750;

    uint seed_ = 42;
    bool debug_ = false;  // TODO(kacper): remove

    [[nodiscard]] static utils::Result<Config> parse(int argc, char* argv[]) noexcept {
        Config config;

        CLI::App app{"OptiX-based raytracer of kernel mixture models"};

        // -- Option group: Image settings
        auto* image_group = app.add_option_group("Image settings");
        image_group->add_option("--output", config.output_path_, "Path to save the rendered image");
        image_group->add_option("--height", config.image_height_,
                                "Explicit height of the output image");
        image_group->add_option("--width", config.image_width_, "Width of the output image");
        image_group->add_option("--samples_per_pixel", config.num_samples_per_pixel_,
                                "Number of samples per pixel");

        // -- Option group: Entrypoint names
        auto* entry_group = app.add_option_group("Entrypoint names");
        entry_group->add_option("--raygen", config.raygen_function_name_,
                                "Name of raygen function");
        entry_group->add_option("--miss", config.miss_function_name_, "Name of miss function");
        entry_group->add_option("--anyhit", config.anyhit_function_name_,
                                "Name of anyhit function");
        entry_group->add_option("--launch_params", config.launch_params_variable_name_,
                                "Launch parameters variable name");

        // -- Option group: File paths
        auto* file_group = app.add_option_group("File paths");
        file_group->add_option("--module_blob", config.module_blob_path_,
                               "Path to the Optix-IR file");
        file_group->add_option("--env_map", config.env_map_path_, "Path to the environment map");

        // -- Option group: Runtime tweaks
        auto* runtime_group = app.add_option_group("Runtime tweaks");
        runtime_group->add_option("--seed", config.seed_, "Random seed");
        runtime_group->add_option("--debug", config.debug_, "Enable debug output");

        argv = app.ensure_utf8(argv);
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            std::ostringstream oss;
            auto code = app.exit(e, oss);
            return utils::make_error("Argument parse error (code {}): {}", code, oss.str());
        }

        return config;
    }
};

}  // namespace thesis::host::app
