#include "thesis/pch.h"

#include "thesis/host/app/renderer.h"
#include "thesis/host/app/config.h"
#include "thesis/host/app/logging.h"
#include "thesis/host/utils/result.h"

#include <optix_function_table_definition.h>

#include <utility>

using namespace thesis;

int main(int argc, char* argv[]) {
    logging::initLogging();

    auto config = core::try_unwrap_or_exit(app::Config::parse(argc, argv));
    Renderer renderer(config);
    renderer.render();

    return 0;
}
