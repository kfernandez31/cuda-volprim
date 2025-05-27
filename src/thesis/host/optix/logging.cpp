#include "thesis/host/optix/logging.h"

#include "thesis/pch.h"

#include "thesis/common/utils/types.h"

#include <array>
#include <functional>
#include <spdlog/spdlog.h>

namespace thesis::optix {

void contextLogCb(uint level, const char* /*tag*/, const char* message, void* /*cbdata*/) {
    using Logger = std::function<void(const char*)>;
    static const std::array<Logger, 5> loggers = {
        [](auto msg) { spdlog::critical("OptiX: {}", msg); },
        [](auto msg) { spdlog::error("OptiX: {}", msg); },
        [](auto msg) { spdlog::warn("OptiX: {}", msg); },
        [](auto msg) { spdlog::info("OptiX: {}", msg); },
        [](auto msg) { spdlog::debug("OptiX: {}", msg); },
    };

    if (level >= static_cast<unsigned char>(LogLevel::Fatal) &&
        level <= static_cast<unsigned char>(LogLevel::Print)) {
        loggers[level - 1](message);
    } else {
        spdlog::warn("OptiX: Unknown log level {} — {}", level, message);
    }
}

}  // namespace thesis::optix
