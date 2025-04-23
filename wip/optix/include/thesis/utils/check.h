#pragma once

#ifdef __cplusplus

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <optix_types.h>

#include <array>  // IWYU pragma: keep
#include <cstdlib>
#include <driver_types.h>
#include <iostream>

namespace thesis {

namespace logging {
constexpr size_t MAX_LOG_SIZE = 2048;
}  // namespace logging

template <bool exit_on_error = true>
inline void cudaCheck(cudaError_t err, const char* file, int line) noexcept {
    if (err != cudaSuccess) {
        std::cerr << "CUDA Error at " << file << ":" << line << ": " << cudaGetErrorString(err)
                  << '\n';
        if constexpr (exit_on_error) {
            std::exit(1);
        }
    }
}

template <bool exit_on_error = true>
inline void optixCheck(OptixResult res, const char* file, int line) noexcept {
    if (res != OPTIX_SUCCESS) {
        std::cerr << "OptiX Error at " << file << ":" << line << ": code " << static_cast<int>(res)
                  << '\n';
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
        std::cerr << "CUDA Driver API Error at " << file << ":" << line << ": " << err_str << '\n';
        if constexpr (exit_on_error) {
            std::exit(1);
        }
    }
}

}  // namespace thesis

#define CUDA_CHECK(call) thesis::cudaCheck<true>((call), __FILE__, __LINE__)
#define CUDA_CHECK_NOEXCEPT(call) thesis::cudaCheck<false>((call), __FILE__, __LINE__)

#define OPTIX_CHECK(call) thesis::optixCheck<true>((call), __FILE__, __LINE__)
#define OPTIX_CHECK_NOEXCEPT(call) thesis::optixCheck<false>((call), __FILE__, __LINE__)

#define CU_CHECK(call) thesis::cuCheck<true>((call), __FILE__, __LINE__)
#define CU_CHECK_NOEXCEPT(call) thesis::cuCheck<false>((call), __FILE__, __LINE__)

#define OPTIX_CALL_LOGGED(call)                                                              \
    do {                                                                                     \
        std::array<char, thesis::logging::MAX_LOG_SIZE> log = {};                            \
        size_t log_size = thesis::logging::MAX_LOG_SIZE;                                     \
        const OptixResult res = (call);                                                      \
        if (log_size > 1 && log[0] != '\0')                                                  \
            std::cerr << "OptiX Log: " << log.data() << '\n';                                \
        if (res != OPTIX_SUCCESS) {                                                          \
            std::cerr << "OptiX Error: " << static_cast<int>(res) << " (" << __FILE__ << ":" \
                      << __LINE__ << ")\n";                                                  \
            std::exit(1);                                                                    \
        }                                                                                    \
    } while (0)

#endif  // __cplusplus
