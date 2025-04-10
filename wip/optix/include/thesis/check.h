#pragma once

#include <cuda_runtime.h>
#include <optix.h>

#include <iostream>

#define CUDA_CHECK( call )                                                       \
    do {                                                                         \
        cudaError_t err = call;                                                  \
        if (err != cudaSuccess)  {                                               \
            std::cerr << "CUDA Error: " << cudaGetErrorString(err) << std::endl; \
            exit(1);                                                             \
        }                                                                        \
    } while(0)

#define OPTIX_CHECK( call )                                   \
    do {                                                      \
        OptixResult res = call;                               \
        if (res != OPTIX_SUCCESS)  {                          \
            std::cerr << "OptiX Error: " << res << std::endl; \
            exit(1);                                          \
        }                                                     \
    } while(0)

#define CU_CHECK( call )                                               \
do {                                                                   \
    CUresult err = call;                                               \
    if (err != CUDA_SUCCESS) {                                         \
        const char* errStr;                                            \
        cuGetErrorString(err, &errStr);                                \
        std::cerr << "CUDA Driver API Error: " << errStr << std::endl; \
        exit(1);                                                       \
    }                                                                  \
} while (0)

#define LOG_SIZE 2048

#define OPTIX_CALL_LOGGED(call)                                        \
    do {                                                               \
        char log[LOG_SIZE];                                            \
        size_t logSize = LOG_SIZE;                                     \
        OptixResult res = call;                                        \
        if (logSize > 1 && log[0] != '\0')                             \
            std::cerr << "OptiX Log: " << log << std::endl;            \
        if (res != OPTIX_SUCCESS) {                                    \
            std::cerr << "OptiX Error: " << static_cast<int>(res)      \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            exit(1);                                                   \
        }                                                              \
    } while (0)
