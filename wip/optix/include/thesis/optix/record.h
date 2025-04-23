#pragma once

#ifdef __cplusplus

#include "thesis/utils/check.h"

#include <cuda.h>
#include <optix_host.h>
#include <optix_stubs.h>
#include <optix_types.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <utility>

namespace thesis::optix {

template <typename T>
class Record {
   public:
    explicit Record(OptixProgramGroup program, const T* data = nullptr) {
        static_assert(sizeof(T) <= OPTIX_SBT_RECORD_HEADER_SIZE, "T too large for SBT record");

        CU_CHECK(cuMemAlloc(&device_ptr_, OPTIX_SBT_RECORD_HEADER_SIZE));

        std::array<std::byte, OPTIX_SBT_RECORD_HEADER_SIZE + sizeof(T)> host_record = {};
        OPTIX_CHECK(optixSbtRecordPackHeader(program, host_record.data()));

        if (data) {
            std::memcpy(reinterpret_cast<void*>(host_record.data() + OPTIX_SBT_RECORD_HEADER_SIZE),
                        data, sizeof(T));
        }

        CU_CHECK(cuMemcpyHtoD(device_ptr_, host_record.data(), OPTIX_SBT_RECORD_HEADER_SIZE));
    }

    ~Record() noexcept { CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_)); }

    // Delete copy
    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    // Enable move ctor
    Record(Record&& other) noexcept : device_ptr_(std::exchange(other.device_ptr_, 0)) {}

    // Enable move assignment
    Record& operator=(Record&& other) noexcept {
        if (this != &other) {
            CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
        }
        return *this;
    }

    [[nodiscard]] const CUdeviceptr& get() const noexcept { return device_ptr_; }
    [[nodiscard]] CUdeviceptr& get() noexcept { return device_ptr_; }

   private:
    CUdeviceptr device_ptr_ = 0;
};

template <>
class Record<void> {
   public:
    explicit Record(OptixProgramGroup program) {
        CU_CHECK(cuMemAlloc(&device_ptr_, OPTIX_SBT_RECORD_HEADER_SIZE));

        std::array<std::byte, OPTIX_SBT_RECORD_HEADER_SIZE> host_record = {};
        OPTIX_CHECK(optixSbtRecordPackHeader(program, host_record.data()));

        CU_CHECK(cuMemcpyHtoD(device_ptr_, host_record.data(), OPTIX_SBT_RECORD_HEADER_SIZE));
    }

    ~Record() noexcept { CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_)); }

    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    Record(Record&& other) noexcept : device_ptr_(std::exchange(other.device_ptr_, 0)) {}

    Record& operator=(Record&& other) noexcept {
        if (this != &other) {
            CU_CHECK_NOEXCEPT(cuMemFree(device_ptr_));
            device_ptr_ = std::exchange(other.device_ptr_, 0);
        }
        return *this;
    }

    [[nodiscard]] const CUdeviceptr& get() const noexcept { return device_ptr_; }
    [[nodiscard]] CUdeviceptr& get() noexcept { return device_ptr_; }

   private:
    CUdeviceptr device_ptr_ = 0;
};

}  // namespace thesis::optix

#endif  // __cplusplus
