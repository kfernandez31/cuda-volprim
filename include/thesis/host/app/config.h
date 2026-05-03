#pragma once

#include "thesis/common/utils/types.h"
#include "thesis/host/utils/result.h"

#include <cstddef>
#include <filesystem>
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

    size_t num_samples_per_pixel_ = 1;
    size_t image_width_ = 1000;
    size_t image_height_ = 750;

    uint seed_ = 42;
    bool denoise_ = false;

    [[nodiscard]] static utils::Result<Config> parse(int argc, char* argv[]) noexcept;
};

}  // namespace thesis::host::app
