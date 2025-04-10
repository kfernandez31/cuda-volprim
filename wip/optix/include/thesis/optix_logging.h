#pragma once

#include <string_view>

// OptiX log levels (matching internal behavior)
constexpr unsigned int OPTIX_LOG_LEVEL_NONE    = 0;
constexpr unsigned int OPTIX_LOG_LEVEL_FATAL   = 1;
constexpr unsigned int OPTIX_LOG_LEVEL_ERROR   = 2;
constexpr unsigned int OPTIX_LOG_LEVEL_WARNING = 3;
constexpr unsigned int OPTIX_LOG_LEVEL_PRINT   = 4; // Sometimes labeled "INFO" or "ALL"
constexpr unsigned int OPTIX_LOG_LEVEL_ALL     = OPTIX_LOG_LEVEL_PRINT;

namespace thesis {

void context_log_cb(unsigned int level, const char* tag, const char* message, void* cbdata);

} // namespace thesis
