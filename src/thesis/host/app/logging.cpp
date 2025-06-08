#include "thesis/host/app/logging.h"

#include "thesis/pch.h"

#include <spdlog/spdlog.h>

namespace thesis::host::app::logging {

void initLogging() {
#ifdef DEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif  // DEBUG
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info("Starting application, logging enabled.");
}

}  // namespace thesis::host::app::logging
