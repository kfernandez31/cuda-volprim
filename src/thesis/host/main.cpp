#include "thesis/pch.h"

#include "thesis/host/app/config.h"
#include "thesis/host/app/logging.h"
#include "thesis/host/app/renderer.h"
#include "thesis/host/utils/result.h"

#ifndef OPTIX_FUNCTION_TABLE_INCLUDED
#define OPTIX_FUNCTION_TABLE_INCLUDED
#include <optix_function_table_definition.h>
#endif  // OPTIX_FUNCTION_TABLE_INCLUDED

#include <utility>

using namespace thesis::host;

int main(int argc, char* argv[]) {
    app::logging::init();

    auto config = utils::try_unwrap_or_exit(app::Config::parse(argc, argv));
    app::Renderer renderer(config);
    renderer.render();

    return 0;
}