#pragma once

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <optix_types.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace thesis::host::utils {

constexpr size_t MAX_LOG_SIZE = 2048;

template <bool exit_on_error = true>
void cudaCheck(cudaError_t err, const char* file, int line) noexcept {
    if (err != cudaSuccess) {
        spdlog::error("CUDA Error at {}:{}: {}", file, line, cudaGetErrorString(err));
        if constexpr (exit_on_error) {
            std::exit(1);
        }
    }
}

template <bool exit_on_error = true>
void optixCheck(OptixResult res, const char* file, int line) noexcept {
    if (res != OPTIX_SUCCESS) {
        spdlog::error("OptiX Error at {}:{}: code {}", file, line, static_cast<int>(res));
        if constexpr (exit_on_error) {
            std::exit(1);
        }
    }
}

template <bool exit_on_error = true>
void cuCheck(CUresult err, const char* file, int line) noexcept {
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
void checkNotNull(const T* ptr, const char* expr, const char* file, int line,
                  const char* msg = nullptr) noexcept {
    if (ptr) {
        return;
    }

    if (msg) {
        spdlog::error("Null pointer check failed at {}:{} → '{}': {}", file, line, expr, msg);
    } else {
        spdlog::error("Null pointer check failed at {}:{} → '{}'", file, line, expr);
    }
    std::exit(1);
}

}  // namespace thesis::host::utils

#define CUDA_CHECK(call) thesis::host::utils::cudaCheck<true>((call), __FILE__, __LINE__)
#define CUDA_CHECK_NOEXCEPT(call) thesis::host::utils::cudaCheck<false>((call), __FILE__, __LINE__)

#define OPTIX_CHECK(call) thesis::host::utils::optixCheck<true>((call), __FILE__, __LINE__)
#define OPTIX_CHECK_NOEXCEPT(call) \
    thesis::host::utils::optixCheck<false>((call), __FILE__, __LINE__)

#define CU_CHECK(call) thesis::host::utils::cuCheck<true>((call), __FILE__, __LINE__)
#define CU_CHECK_NOEXCEPT(call) thesis::host::utils::cuCheck<false>((call), __FILE__, __LINE__)

#define CHECK_NOT_NULL(ptr, ...) \
    thesis::host::utils::checkNotNull((ptr), #ptr, __FILE__, __LINE__, ##__VA_ARGS__)

#define OPTIX_CALL_LOGGED(call)                                                                  \
    do {                                                                                         \
        std::array<char, thesis::host::utils::MAX_LOG_SIZE> log = {};                            \
        size_t log_size = log.size();                                                            \
        const OptixResult res = (call);                                                          \
        if (log_size > 1 && log[0] != '\0') {                                                    \
            spdlog::debug("OptiX Log: {}", log.data());                                          \
        }                                                                                        \
        if (res != OPTIX_SUCCESS) {                                                              \
            spdlog::error("OptiX Error: {} ({}:{})", static_cast<int>(res), __FILE__, __LINE__); \
            std::exit(1);                                                                        \
        }                                                                                        \
    } while (0)
