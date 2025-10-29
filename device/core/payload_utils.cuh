#pragma once

#include <optix.h>
#include <cstdint>

namespace thesis {
namespace device {

__device__ inline void pack_ptr(void* ptr, uint32_t& p0, uint32_t& p1) {
    const uint64_t uptr = reinterpret_cast<uint64_t>(ptr);
    p0 = static_cast<uint32_t>(uptr);
    p1 = static_cast<uint32_t>(uptr >> 32);
}

template <typename T>
__device__ inline T* unpack_ptr(uint32_t p0, uint32_t p1) {
    const uint64_t uptr = (static_cast<uint64_t>(p1) << 32) | static_cast<uint64_t>(p0);
    return reinterpret_cast<T*>(uptr);
}

} // namespace device
} // namespace thesis
