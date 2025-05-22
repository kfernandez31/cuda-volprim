// #include "thesis/host/pch.h"

#include "thesis/host/utils/app_config.h"
#include "thesis/host/utils/result.h"
#include "thesis/host/renderer.h"
#include "thesis/host/utils/logging.h"

#include <optix_function_table_definition.h>

#include <utility>

int main(int argc, char* argv[]) {
    thesis::logging::initLogging();

    auto config = thesis::core::try_unwrap_or_exit(thesis::AppConfig::parse(argc, argv));
    thesis::Renderer renderer(std::move(config));
    renderer.render();

    return 0;
}
