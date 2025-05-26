#pragma once

#include "thesis/host/utils/result.h"
#include "thesis/common/utils/types.h"

#include <CLI11/CLI11.hpp>
#include <cstddef>
#include <filesystem>
#include <sstream>
#include <string>

namespace thesis {

namespace fs = std::filesystem;

struct AppConfig {
    std::string raygen_function_name_ = "__raygen__rg";
    std::string miss_function_name_ = "__miss__ms";
    std::string closesthit_function_name_ = "__closesthit__ch";
    std::string anyhit_function_name = "__anyhit__ah";
    std::string launch_params_variable_name_ = "params";

    fs::path output_path_ = "output.exr";
    fs::path ptx_path_ = fs::path("build") / "device_program.ptx";
    fs::path env_map_path_ = fs::path("assets") / "meadow_2_4k.hdr";

    size_t num_samples_per_pixel_ = 100;
    size_t image_width_ = 1200;
    size_t image_height_ = 900;
    float aspect_ratio_ = static_cast<float>(image_width_) / image_height_;

    uint seed_ = 42;

    static core::Result<AppConfig> parse(int argc, char* argv[]) noexcept {
        AppConfig result;

        CLI::App app{"OptiX-based raytracer of kernel mixture models"};
        app.add_option("--output", result.output_path_, "Path to save the rendered image");
        app.add_option("--ptx", result.ptx_path_, "Path to the PTX file");
        app.add_option("--env_map", result.env_map_path_, "Path to the environment map");
        app.add_option("--samples_per_pixel", result.num_samples_per_pixel_,
                       "Number of samples per pixel");
        app.add_option("--width", result.image_width_, "Width of the output image");
        app.add_option("--raygen", result.raygen_function_name_, "Name of raygen function");
        app.add_option("--miss", result.miss_function_name_, "Name of miss function");
        app.add_option("--closesthit", result.closesthit_function_name_,
                       "Name of closesthit function");
        app.add_option("--launch_params", result.launch_params_variable_name_,
                       "Launch parameters variable name");
        app.add_option("--seed", result.seed_, "Random seed");

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
            return core::make_error("Argument parse error (code {}): {}", code, oss.str());
        }

        if (height_opt && aspect_opt) {
            return core::make_error("Only one of --height or --aspect_ratio may be specified.");
        }

        if (height_opt) {
            result.image_height_ = *height_opt;
            result.aspect_ratio_ = static_cast<float>(result.image_width_) / result.image_height_;
        } else if (aspect_opt) {
            result.aspect_ratio_ = *aspect_opt;
            result.image_height_ =
                static_cast<size_t>(static_cast<float>(result.image_width_) / result.aspect_ratio_);
        }

        return result;
    }
};

}  // namespace thesis