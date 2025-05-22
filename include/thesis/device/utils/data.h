#pragma once

#include <cstdint>

namespace thesis {
namespace device {
namespace data {

__forceinline__ __device__ void *unpackPointer(uint32_t i0, uint32_t i1) noexcept {
    const auto uptr = static_cast<uint64_t>(i0) << 32 | i1;
    auto* ptr = reinterpret_cast<void*>(uptr);
    return ptr;
}

__forceinline__ __device__ void packPointer(void* ptr, uint32_t& i0, uint32_t& i1) noexcept {
    const auto uptr = reinterpret_cast<uint64_t>(ptr);
    i0 = uptr >> 32;
    i1 = uptr & 0x00000000ffffffff;
}

} // namespace data
} // namespace device
} // namespace thesis
