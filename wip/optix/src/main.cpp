#include "thesis/pch.h"

#include "thesis/app_config.h"
#include "thesis/renderer.h"

#include <spdlog/spdlog.h>

#include <optix_function_table_definition.h>

#include <string>
#include <cstdlib>

#include <optional>
#include <string>

#include "thesis/utils/logging.h"

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
