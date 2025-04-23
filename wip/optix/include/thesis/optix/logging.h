#pragma once

#ifdef __cplusplus

namespace thesis::optix
{

// OptiX log levels (matching internal behavior)
enum class LogLevel : unsigned int
{
    None = 0,
    Fatal = 1,
    Error = 2,
    Warning = 3,
    Print = 4,
    All = Print,
    Info = Print,
};

void contextLogCb(unsigned int level, const char* tag, const char* message, void* cbdata);

}  // namespace thesis::optix

#endif  // __cplusplus
