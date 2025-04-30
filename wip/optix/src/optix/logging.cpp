#include "thesis/optix/logging.h"

#include "thesis/thesis_pch.h"

#include <spdlog/spdlog.h>

#include <array>
#include <functional>
#include <string_view>

namespace thesis::optix {

void contextLogCb(unsigned int level, const char* /*tag*/, const char* message, void* /*cbdata*/) {
    using Logger = std::function<void(std::string_view)>;
    static const std::array<Logger, 5> loggers = {
        [](auto msg) { spdlog::critical("OptiX: {}", msg); },
        [](auto msg) { spdlog::error("OptiX: {}", msg); },
        [](auto msg) { spdlog::warn("OptiX: {}", msg); },
        [](auto msg) { spdlog::info("OptiX: {}", msg); },
        [](auto msg) { spdlog::debug("OptiX: {}", msg); },
    };

    if (level >= static_cast<unsigned int>(LogLevel::Fatal) &&
        level <= static_cast<unsigned int>(LogLevel::Print)) {
        loggers[level - 1](message);
    } else {
        spdlog::warn("OptiX: Unknown log level {} — {}", level, message);
    }
}

}  // namespace thesis::optix
