#pragma once

#include <string>
#include <string_view>
#include <cstddef>
#include <optional>

#include <CLI11/CLI11.hpp>

namespace thesis {

struct AppConfig {
    // non-configurable
    static constexpr std::string_view raygen_function_name = "__raygen__rg";
    static constexpr std::string_view miss_function_name = "__miss__ms";
    static constexpr std::string_view hitgroup_function_name = "__closesthit__ch";
    static constexpr std::string_view launch_params_variable_name_ = "params";

    // configurable // TODO(kacper): _
    std::string output_path_ = "output.exr";
    std::string ptx_path_ = "build/device_program.ptx";
    std::string env_map_path_ = "assets/meadow_2_4k.hdr";
    size_t num_samples_per_pixel_ = 10;
    size_t image_width_ = 800;
    float aspect_ratio_ = 16.0f / 9.0f;

    AppConfig() = default;

    AppConfig(const AppConfig&) = default;
    AppConfig& operator=(const AppConfig&) = default;

    AppConfig(AppConfig&& other) = default;
    AppConfig& operator=(AppConfig&& other) = default;

    std::optional<std::pair<int, std::string>> parse(int argc, char* argv[]) {
        CLI::App app{"OptiX-based raytracer of kernel mixture models"};
        app.add_option("-o,--output", output_path_, "Path to save the rendered image")->required(false);
        app.add_option("-p,--ptx", ptx_path_, "Path to the PTX file")->required(false);
        app.add_option("-e,--env_map", env_map_path_, "Path to the environment map")->required(false);
        app.add_option("-s,--num_samples", num_samples_per_pixel_, "Samples per pixel")->required(false);
        app.add_option("-w,--width", image_width_, "Width of output image")->required(false);
        app.add_option("-a,--aspect_ratio", aspect_ratio_, "Aspect ratio of output image")->required(false);

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

} // namespace thesis
