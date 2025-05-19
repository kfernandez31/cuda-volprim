#pragma once

#include <CLI11/CLI11.hpp>
#include <cstddef>
#include <filesystem>
#include <optional>
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

    size_t num_samples_per_pixel_ = 10;
    size_t image_width_ = 800;
    size_t image_height_ = 800;
    float aspect_ratio_ = 16.0f / 9.0f; // TODO(kacper): this xor height, perhaps achieved with std::optional

    AppConfig() = default;

    AppConfig(const AppConfig&) = default;
    AppConfig& operator=(const AppConfig&) = default;

    AppConfig(AppConfig&& other) noexcept = default;
    AppConfig& operator=(AppConfig&& other) noexcept = default;

    std::optional<std::pair<int, std::string>> parse(int argc, char* argv[]) {
        CLI::App app{"OptiX-based raytracer of kernel mixture models"};
        app.add_option("-o,--output", output_path_, "Path to save the rendered image")
            ->required(false);
        app.add_option("-p,--ptx", ptx_path_, "Path to the PTX file")->required(false);
        app.add_option("-e,--env_map", env_map_path_, "Path to the environment map")
            ->required(false);
        app.add_option("-s,--num_samples", num_samples_per_pixel_, "Number of samples per pixel")
            ->required(false);
        app.add_option("-w,--width", image_width_, "Width of the output image")->required(false);
        app.add_option("-a,--aspect_ratio", aspect_ratio_, "Aspect ratio of output image")
            ->required(false);
        app.add_option("-r,--raygen", raygen_function_name_,
                       "Name of the raygen function in the PTX")
            ->required(false);
        app.add_option("-m,--miss", miss_function_name_, "Name of the miss function in the PTX")
            ->required(false);
        app.add_option("-c,--closesthit", closesthit_function_name_,
                       "Name of the hitgroup function in the PTX")
            ->required(false);
        app.add_option("-l,--launch_params", launch_params_variable_name_,
                       "Name of the launch params variable in the PTX")
            ->required(false);

        argv = app.ensure_utf8(argv);
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            std::ostringstream oss;
            auto code = app.exit(e, oss);
            return std::make_pair(code, oss.str());
        }
        return {};
    }
};

}  // namespace thesis
