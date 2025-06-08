#include "thesis/host/app/logging.h"

#include "thesis/pch.h"

#include <spdlog/spdlog.h>

namespace thesis::logging {

void initLogging() {
#ifdef DEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif  // DEBUG
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info("Starting OptiX application");
}

}  // namespace thesis::logging
