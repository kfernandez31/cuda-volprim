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

    // -- Option group: Render parameters (was compile-time in constants.cuh)
    auto* render_group = app.add_option_group("Render parameters");
    render_group->add_option("--max-depth", config.max_bounces_, "Maximum path depth (bounces)")->check(CLI::PositiveNumber);
    render_group->add_option("--rr-depth", config.rr_depth_, "Depth at which Russian roulette begins");
    render_group->add_option("--rr-max-survival", config.rr_max_survival_, "Russian roulette max survival probability")->check(CLI::Range(0.0f, 1.0f));
    render_group->add_option("--firefly-clamp", config.firefly_clamp_luminance_, "Per-sample luminance clamp (0=off; BIASED when >0)")->check(CLI::NonNegativeNumber);
    render_group->add_option("--filter-width", config.pixel_filter_stddev_, "Gaussian pixel-filter width (stddev) in px (0=box)")->check(CLI::NonNegativeNumber);
    render_group->add_option("--phase-g", config.hg_g_, "Henyey-Greenstein phase anisotropy g in (-1,1)")->check(CLI::Range(-0.999f, 0.999f));
    render_group->add_flag("--measure-caps", config.measure_caps_,
        "Measure max hits/ray and point-overlap during the render and print suggested caps");
    // clang-format on

    argv = app.ensure_utf8(argv);
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::ostringstream oss;
        auto code = app.exit(e, oss);
        return utils::make_error("Argument parse error (code {}): {}", code, oss.str());
    } catch (const std::exception& e) {
        // parse() can throw non-ParseError types; this function is noexcept, so letting one
        // escape would std::terminate. Surface it as a Result error instead.
        return utils::make_error("Argument parsing failed: {}", e.what());
    } catch (...) {
        return utils::make_error("Argument parsing failed: unknown error");
    }

    return config;
}

}  // namespace thesis::host::app
