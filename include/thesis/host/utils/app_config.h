#pragma once

#include "thesis/host/utils/result.h"

#include <CLI11/CLI11.hpp>
#include <cstddef>
#include <filesystem>
#include <string>
#include <sstream>

namespace thesis {

namespace fs = std::filesystem;

struct AppConfig {
    std::string raygen_function_name_     = "__raygen__rg";
    std::string miss_function_name_       = "__miss__ms";
    std::string closesthit_function_name_ = "__closesthit__ch";
    std::string anyhit_function_name      = "__anyhit__ah";
    std::string launch_params_variable_name_ = "params";

    fs::path output_path_ = "output.exr";
    fs::path ptx_path_    = fs::path("build") / "device_program.ptx";
    fs::path env_map_path_ = fs::path("assets") / "meadow_2_4k.hdr";

    size_t num_samples_per_pixel_ = 10;
    size_t image_width_ = 800;
    size_t image_height_ = 600;
    float aspect_ratio_ = static_cast<float>(image_width_) / image_height_;

    static core::Result<AppConfig> parse(int argc, char* argv[]) noexcept {
        CLI::App app{"OptiX-based raytracer of kernel mixture models"};

        std::optional<size_t> height_opt;
        std::optional<float>  aspect_opt;

        app.add_option("-o,--output", output_path_, "Path to save the rendered image");
        app.add_option("-p,--ptx", ptx_path_, "Path to the PTX file");
        app.add_option("-e,--env_map", env_map_path_, "Path to the environment map");
        app.add_option("-s,--num_samples", num_samples_per_pixel_, "Number of samples per pixel");
        app.add_option("-y,--width", image_width_, "Width of the output image")->required();
        app.add_option("-x,--height", height_opt, "Explicit height of the output image");
        app.add_option("-a,--aspect_ratio", aspect_opt, "Aspect ratio = width / height");
        app.require_option(1, height_opt, aspect_opt);

        app.add_option("-r,--raygen", raygen_function_name_, "Name of raygen function");
        app.add_option("-m,--miss", miss_function_name_, "Name of miss function");
        app.add_option("-c,--closesthit", closesthit_function_name_, "Name of closesthit function");
        app.add_option("-l,--launch_params", launch_params_variable_name_, "Launch param name");

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
            image_height_ = *height_opt;
            aspect_ratio_ = static_cast<float>(image_width_) / image_height_;
        } else if (aspect_opt) {
            aspect_ratio_ = *aspect_opt;
            image_height_ = static_cast<size_t>(static_cast<float>(image_width_) / aspect_ratio_);
        }

        return {};
    }
};

}  // namespace thesis