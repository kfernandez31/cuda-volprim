#include "thesis/pch.h"

#include "thesis/host/app/renderer.h"
#include "thesis/host/app/config.h"
#include "thesis/host/app/logging.h"
#include "thesis/host/utils/result.h"

#include <optix_function_table_definition.h> // important - do not remove or include in another file!

#include <utility>

using namespace thesis::host;

int main(int argc, char* argv[]) {
    app::logging::initLogging();

    auto config = try_unwrap_or_exit(app::Config::parse(argc, argv));
    app::Renderer renderer(config);
    renderer.render();

    return 0;
}
