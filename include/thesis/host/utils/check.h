#pragma once

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <optix_types.h>

#include <array>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace thesis {

namespace logging {
constexpr auto MAX_LOG_SIZE = 2048u;
}  // namespace logging

template <bool exit_on_error = true>
inline void cudaCheck(cudaError_t err, const char* file, int line) noexcept {
    if (err != cudaSuccess) {
        spdlog::error("CUDA Error at {}:{}: {}", file, line, cudaGetErrorString(err));
        if constexpr (exit_on_error) {
            std::exit(1);
        }
    }
}

template <bool exit_on_error = true>
inline void optixCheck(OptixResult res, const char* file, int line) noexcept {
    if (res != OPTIX_SUCCESS) {
        spdlog::error("OptiX Error at {}:{}: code {}", file, line, static_cast<int>(res));
        if constexpr (exit_on_error) {
            std::exit(1);
        }
    }
}

template <bool exit_on_error = true>
inline void cuCheck(CUresult err, const char* file, int line) noexcept {
    if (err != CUDA_SUCCESS) {
        const char* err_str = nullptr;
        cuGetErrorString(err, &err_str);
        spdlog::error("CUDA Driver API Error at {}:{}: {}", file, line, err_str);
        if constexpr (exit_on_error) {
            std::exit(1);
        }
    }
}

template <typename T>
inline void checkNotNull(const T* ptr, const char* expr, const char* file, int line,
                         const char* msg = nullptr) {
    if (!ptr) {
        if (msg) {
            spdlog::error("Null pointer check failed at {}:{} → '{}': {}", file, line, expr, msg);
        } else {
            spdlog::error("Null pointer check failed at {}:{} → '{}'", file, line, expr);
        }
        std::exit(1);
    }
}

}  // namespace thesis

#define CUDA_CHECK(call) thesis::cudaCheck<true>((call), __FILE__, __LINE__)
#define CUDA_CHECK_NOEXCEPT(call) thesis::cudaCheck<false>((call), __FILE__, __LINE__)

#define OPTIX_CHECK(call) thesis::optixCheck<true>((call), __FILE__, __LINE__)
#define OPTIX_CHECK_NOEXCEPT(call) thesis::optixCheck<false>((call), __FILE__, __LINE__)

#define CU_CHECK(call) thesis::cuCheck<true>((call), __FILE__, __LINE__)
#define CU_CHECK_NOEXCEPT(call) thesis::cuCheck<false>((call), __FILE__, __LINE__)

#define CHECK_NOT_NULL(ptr, ...) \
    thesis::checkNotNull((ptr), #ptr, __FILE__, __LINE__, ##__VA_ARGS__)

#define OPTIX_CALL_LOGGED(call)                                                                  \
    do {                                                                                         \
        std::array<char, thesis::logging::MAX_LOG_SIZE> log = {};                                \
        size_t log_size = thesis::logging::MAX_LOG_SIZE;                                         \
        const OptixResult res = (call);                                                          \
        if (log_size > 1 && log[0] != '\0') {                                                    \
            spdlog::debug("OptiX Log: {}", log.data());                                          \
        }                                                                                        \
        if (res != OPTIX_SUCCESS) {                                                              \
            spdlog::error("OptiX Error: {} ({}:{})", static_cast<int>(res), __FILE__, __LINE__); \
            std::exit(1);                                                                        \
        }                                                                                        \
    } while (0)
