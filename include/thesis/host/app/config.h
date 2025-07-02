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
    std::string closesthit_function_name_ = "__closesthit__ch";
    std::string anyhit_function_name = "__anyhit__ah";
    std::string launch_params_variable_name_ = "launch_params";

    fs::path output_path_ = "output.exr";
    fs::path module_blob_path_ = fs::path("build") / "device_program.optixir";
    fs::path env_map_path_ = fs::path("assets") / "meadow_2_4k.hdr";

    size_t num_samples_per_pixel_ = 16;
    size_t image_width_ = 2000;
    size_t image_height_ = 1500;
    float aspect_ratio_ = static_cast<float>(image_width_) / image_height_;

    uint seed_ = 42;

    [[nodiscard]] static utils::Result<Config> parse(int argc, char* argv[]) noexcept {
        Config config;

        CLI::App app{"OptiX-based raytracer of kernel mixture models"};
        {
            // clang-format off
            app.add_option("--output", config.output_path_, "Path to save the rendered image");
            app.add_option("--module_blob", config.module_blob_path_, "Path to the Optix-IR file");
            app.add_option("--env_map", config.env_map_path_, "Path to the environment map");
            app.add_option("--samples_per_pixel", config.num_samples_per_pixel_, "Number of samples per pixel");
            app.add_option("--width", config.image_width_, "Width of the output image");
            app.add_option("--raygen", config.raygen_function_name_, "Name of raygen function");
            app.add_option("--miss", config.miss_function_name_, "Name of miss function");
            app.add_option("--closesthit", config.closesthit_function_name_, "Name of closesthit function");
            app.add_option("--launch_params", config.launch_params_variable_name_, "Launch parameters variable name");
            app.add_option("--seed", config.seed_, "Random seed");
        }

        std::optional<size_t> height_opt;
        std::optional<float> aspect_opt;
        app.add_option("--height", height_opt, "Explicit height of the output image");
        app.add_option("--aspect_ratio", aspect_opt, "Aspect ratio = width / height");

        argv = app.ensure_utf8(argv);
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            std::ostringstream oss;
            auto code = app.exit(e, oss);
            return utils::make_error("Argument parse error (code {}): {}", code, oss.str());
        }

        if (height_opt && aspect_opt) {
            return utils::make_error("Only one of --height or --aspect_ratio may be specified.");
        }

        if (height_opt) {
            config.image_height_ = *height_opt;
            config.aspect_ratio_ = static_cast<float>(config.image_width_) / config.image_height_;
        } else if (aspect_opt) {
            config.aspect_ratio_ = *aspect_opt;
            config.image_height_ =
                static_cast<size_t>(static_cast<float>(config.image_width_) / config.aspect_ratio_);
        }

        return config;
    }
};

}  // namespace thesis::host::app
