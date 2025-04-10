#pragma once

#include "check.h"

#include <cuda_runtime.h>
#include <optix_function_table_definition.h>
#include <optix_host.h>
#include <optix_stubs.h>

#include <cstring>
#include <utility>

namespace thesis {

template <typename T>
class OptixRecord {
public:
    OptixRecord(OptixProgramGroup program, const T* data = nullptr)
    {
        static_assert(sizeof(T) <= OPTIX_SBT_RECORD_HEADER_SIZE, "T too large for SBT record");

        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_ptr_), OPTIX_SBT_RECORD_HEADER_SIZE));

        // Pack record on host
        std::byte host_record[OPTIX_SBT_RECORD_HEADER_SIZE] = {};
        OPTIX_CHECK(optixSbtRecordPackHeader(program, host_record));

        if (data)
            std::memcpy(host_record + OPTIX_SBT_RECORD_HEADER_SIZE, data, sizeof(T));
        CUDA_CHECK(cudaMemcpy(reinterpret_cast<void*>(device_ptr_), host_record, OPTIX_SBT_RECORD_HEADER_SIZE, cudaMemcpyHostToDevice));
    }

    ~OptixRecord() noexcept
    {
        CUDA_CHECK(cudaFree(reinterpret_cast<void*>(device_ptr_)));
    }

    // Delete copy
    OptixRecord(const OptixRecord&) = delete;
    OptixRecord& operator=(const OptixRecord&) = delete;

    // Enable move ctor
    OptixRecord(OptixRecord&& other) noexcept
        : device_ptr_(std::exchange(other.device_ptr_, 0)) {}

    // Enable move assignment
    OptixRecord& operator=(OptixRecord&& other) noexcept
    {
        if (this != &other) {
            CUDA_CHECK(cudaFree(reinterpret_cast<void*>(device_ptr_)));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
        }
        return *this;
    }

    CUdeviceptr get() const noexcept { return device_ptr_; }
    operator CUdeviceptr() const noexcept { return device_ptr_; }

private:
    CUdeviceptr device_ptr_ = 0;
};

template <>
class OptixRecord<void> {
public:
    OptixRecord(OptixProgramGroup program)
    {
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_ptr_), OPTIX_SBT_RECORD_HEADER_SIZE));

        std::byte host_record[OPTIX_SBT_RECORD_HEADER_SIZE] = {};
        OPTIX_CHECK(optixSbtRecordPackHeader(program, host_record));

        CUDA_CHECK(cudaMemcpy(reinterpret_cast<void*>(device_ptr_), host_record, OPTIX_SBT_RECORD_HEADER_SIZE, cudaMemcpyHostToDevice));
    }

    ~OptixRecord() noexcept
    {
        CUDA_CHECK(cudaFree(reinterpret_cast<void*>(device_ptr_)));
    }

    OptixRecord(const OptixRecord&) = delete;
    OptixRecord& operator=(const OptixRecord&) = delete;

    OptixRecord(OptixRecord&& other) noexcept
        : device_ptr_(std::exchange(other.device_ptr_, 0)) {}

    OptixRecord& operator=(OptixRecord&& other) noexcept
    {
        if (this != &other) {
            CUDA_CHECK(cudaFree(reinterpret_cast<void*>(device_ptr_)));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
        }
        return *this;
    }

    CUdeviceptr get() const noexcept { return device_ptr_; }
    operator CUdeviceptr() const noexcept { return device_ptr_; }

private:
    CUdeviceptr device_ptr_ = 0;
};

} // namespace thesis
