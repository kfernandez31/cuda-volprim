#include "thesis/optix_logging.h"

#include <spdlog/spdlog.h>

#include <array>
#include <functional>

namespace thesis {

void context_log_cb(unsigned int level, const char* /*tag*/, const char* message, void* /*cbdata*/) {
    using Logger = std::function<void(std::string_view)>;
    static const std::array<Logger, 5> loggers = {
        [](auto msg){ spdlog::critical("OptiX: {}", msg); },
        [](auto msg){ spdlog::error   ("OptiX: {}", msg); },
        [](auto msg){ spdlog::warn    ("OptiX: {}", msg); },
        [](auto msg){ spdlog::info    ("OptiX: {}", msg); },
        [](auto msg){ spdlog::debug   ("OptiX: {}", msg); },
    };

    loggers[level - 1](message);
}

} // namespace thesis
