#include "thesis/pch.h"

#include "thesis/app_config.h"
#include "thesis/result.h"
#include "thesis/renderer.h"
#include "thesis/utils/logging.h"

#include <optix_function_table_definition.h>

#include <utility>

int main(int argc, char* argv[]) {
    thesis::logging::initLogging();

    auto config = try_unwrap_or_exit(thesis::AppConfig::parse(argc, argv));
    thesis::Renderer renderer(std::move(config));
    renderer.render();

    return 0;
}
