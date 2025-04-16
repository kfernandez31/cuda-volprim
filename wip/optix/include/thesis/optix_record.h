#pragma once

#ifdef __cplusplus

#include "check.h"

#include <cuda.h>
#include <optix_types.h>
#include <optix_stubs.h>
#include <optix_host.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <utility>

namespace thesis {

template <typename T>
class OptixRecord {
public:
    explicit OptixRecord(OptixProgramGroup program, const T* data = nullptr)
    {
        static_assert(sizeof(T) <= OPTIX_SBT_RECORD_HEADER_SIZE, "T too large for SBT record");

        CU_CHECK(cuMemAlloc(&device_ptr_, OPTIX_SBT_RECORD_HEADER_SIZE));

        std::array<std::byte, OPTIX_SBT_RECORD_HEADER_SIZE + sizeof(T)> host_record = {};
        OPTIX_CHECK(optixSbtRecordPackHeader(program, host_record.data()));

        if (data) {
            std::memcpy(reinterpret_cast<void*>(host_record.data() + OPTIX_SBT_RECORD_HEADER_SIZE), data, sizeof(T));
        }

        CU_CHECK(cuMemcpyHtoD(device_ptr_, host_record.data(), OPTIX_SBT_RECORD_HEADER_SIZE));
    }

    ~OptixRecord() noexcept
    {
        CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
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
            CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
        }
        return *this;
    }

    [[nodiscard]] const CUdeviceptr& get() const noexcept { return device_ptr_; }
    [[nodiscard]]       CUdeviceptr& get()       noexcept { return device_ptr_; }

private:
    CUdeviceptr device_ptr_ = 0;
};

template <>
class OptixRecord<void> {
public:
    explicit OptixRecord(OptixProgramGroup program)
    {
        CU_CHECK(cuMemAlloc(&device_ptr_, OPTIX_SBT_RECORD_HEADER_SIZE));

        std::array<std::byte, OPTIX_SBT_RECORD_HEADER_SIZE> host_record = {};
        OPTIX_CHECK(optixSbtRecordPackHeader(program, host_record.data()));

        CU_CHECK(cuMemcpyHtoD(device_ptr_, host_record.data(), OPTIX_SBT_RECORD_HEADER_SIZE));
    }

    ~OptixRecord() noexcept
    {
        CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
    }

    OptixRecord(const OptixRecord&) = delete;
    OptixRecord& operator=(const OptixRecord&) = delete;

    OptixRecord(OptixRecord&& other) noexcept
        : device_ptr_(std::exchange(other.device_ptr_, 0)) {}

    OptixRecord& operator=(OptixRecord&& other) noexcept
    {
        if (this != &other) {
            CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
        }
        return *this;
    }

    [[nodiscard]] const CUdeviceptr& get() const noexcept { return device_ptr_; }
    [[nodiscard]]       CUdeviceptr& get()       noexcept { return device_ptr_; }

private:
    CUdeviceptr device_ptr_ = 0;
};

} // namespace thesis

#endif // __cplusplus
