#include "thesis/host/optix/logging.h"

#include "thesis/pch.h"

#include "thesis/common/utils/types.h"

#include <array>
#include <functional>
#include <spdlog/spdlog.h>

namespace thesis::host::optix {

void contextLogCb(uint level, const char* /*tag*/, const char* message, void* /*cbdata*/) {
    using Logger = std::function<void(const char*)>;
    static const std::array<Logger, 5> loggers = {
        [](auto msg) { spdlog::critical("OptiX: {}", msg); },
        [](auto msg) { spdlog::error("OptiX: {}", msg); },
        [](auto msg) { spdlog::warn("OptiX: {}", msg); },
        [](auto msg) { spdlog::info("OptiX: {}", msg); },
        [](auto msg) { spdlog::debug("OptiX: {}", msg); },
    };

    // OptiX delivers level ∈ [1,4]; guard against an out-of-range value indexing OOB.
    if (level < 1 || level > loggers.size()) {
        spdlog::info("OptiX (level {}): {}", level, message);
        return;
    }
    loggers[level - 1](message);
}

}  // namespace thesis::host::optix
