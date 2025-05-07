#include "thesis/cuda/context_handle.h"
#include "thesis/cuda/stream_handle.h"
#include "thesis/host/camera.h"
#include "thesis/host/environment_map.h"
#include "thesis/host/image.h"
#include "thesis/renderer.h"
#include "thesis/optix/gas_handle.h"
#include "thesis/optix/sbt_handle.h"
#include "thesis/optix/handle.h"
#include "thesis/optix/launch_params.h"
#include "thesis/optix/logging.h"
#include "thesis/optix/record.h"
#include "thesis/pch.h"
#include "thesis/app_config.h"
#include "thesis/utils/check.h"
#include "thesis/utils/io.h"

#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <vector_types.h>

#include <array>
#include <cstddef>
#include <glm/glm.hpp>
#include <optional>
#include <span>
#include <spdlog/spdlog.h>
#include <string>


///////

#include "thesis/utils/logging.h"

namespace tcuda = thesis::cuda;
namespace toptix = thesis::optix;
namespace tio = thesis::io;
namespace thost = thesis::host;
namespace tdevice = thesis::device;

namespace {

auto getConfig(int argc, char* argv[]) {
    thesis::AppConfig config_;
    if (auto err = config_.parse(argc, argv)) {
        const auto& [code, msg] = *err;
        spdlog::error("Error parsing app arguments: {}", msg);
        std::exit(code);
    }
    return config_;
}

} // namespace

int main(int argc, char* argv[]) {
    thesis::logging::initLogging();

    auto config_ = getConfig(argc, argv);

    thesis::Renderer renderer(std::move(config_));
    renderer.render();

    return 0;
}
