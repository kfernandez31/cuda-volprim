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

    // Runtime render params (promoted from device/core/constants.cuh; defaults MIRROR it).
    // Defaults reproduce the prior path's math (unbiased; not bit-exact under fast-math).
    size_t max_bounces_ = 128;            // --max-depth
    size_t rr_depth_ = 5;                 // --rr-depth
    float rr_max_survival_ = 0.99f;       // --rr-max-survival
    float firefly_clamp_luminance_ = 0.0f;  // --firefly-clamp (0 = off; BIASED when >0)
    float pixel_filter_stddev_ = 0.0f;    // --filter-stddev (0 = box; >0 = Gaussian AA)
    float hg_g_ = 0.85f;                  // --hg-g (Henyey-Greenstein anisotropy)

    [[nodiscard]] static utils::Result<Config> parse(int argc, char* argv[]) noexcept;
};

}  // namespace thesis::host::app
