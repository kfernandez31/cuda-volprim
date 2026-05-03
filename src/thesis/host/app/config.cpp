#include "thesis/host/app/config.h"

#include "thesis/host/utils/result.h"

#include <CLI11/CLI11.hpp>

#include <sstream>

namespace thesis::host::app {

utils::Result<Config> Config::parse(int argc, char* argv[]) noexcept {
    Config config;

    CLI::App app{"OptiX-based raytracer of kernel mixture models"};

    // -- Option group: Image settings
    // clang-format off
    auto* image_group = app.add_option_group("Image settings");
    image_group->add_option("--output", config.output_path_, "Path to save the rendered image");
    image_group->add_option("--height", config.image_height_, "Explicit height of the output image");
    image_group->add_option("--width", config.image_width_, "Width of the output image");
    image_group->add_option("--samples_per_pixel", config.num_samples_per_pixel_, "Number of samples per pixel");

    // -- Option group: Entrypoint names
    auto* entry_group = app.add_option_group("Entrypoint names");
    entry_group->add_option("--raygen", config.raygen_function_name_, "Name of raygen function");
    entry_group->add_option("--miss", config.miss_function_name_, "Name of miss function");
    entry_group->add_option("--anyhit", config.anyhit_function_name_, "Name of anyhit function");
    entry_group->add_option("--launch_params", config.launch_params_variable_name_, "Launch parameters variable name");

    // -- Option group: File paths
    auto* file_group = app.add_option_group("File paths");
    file_group->add_option("--module_blob", config.module_blob_path_, "Path to the Optix-IR file");
    file_group->add_option("--env_map", config.env_map_path_, "Path to the environment map");

    // -- Option group: Runtime tweaks
    auto* runtime_group = app.add_option_group("Runtime tweaks");
    runtime_group->add_option("--seed", config.seed_, "Random seed");
    runtime_group->add_flag("--denoise", config.denoise_, "Apply OptiX AI denoiser to final image");
    // clang-format on

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

}  // namespace thesis::host::app
