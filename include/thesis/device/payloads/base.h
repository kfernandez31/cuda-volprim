#pragma once

#include "thesis/common/utils/preprocessor.h"
#include "thesis/common/utils/types.h"

#include <optix.h>

namespace thesis {
namespace device {
namespace payloads {

constexpr auto MAX_PAYLOADS = 32u;

enum class Tag : uint {
    Invalid = 0,
    Miss = 1,
    ClosestHit = 2,
    AnyHit = 3,
};

template <typename Derived, Tag T>
struct THESIS_ALIGNMENT Base {
    static constexpr Tag tag_v = T;

#ifdef DEVICE
    __device__ void pack(uint* out) const {
        const auto* self = static_cast<const Derived*>(this);
        out[0] = static_cast<uint>(self->tag_v);
        self->pack_impl(out + 1);
    }

    __device__ void unpack(const uint* in) {
        auto* self = static_cast<Derived*>(this);
        self->unpack_impl(in + 1);
    }

    __device__ void packToOptix() const {
        uint payloads[Derived::Count] = {};
        static_cast<const Derived*>(this)->pack(payloads);
        static_assert(Derived::Count <= MAX_PAYLOADS, "Max number of payloads exceeded.");

#define CASE(n) \
    case (n):   \
        optixSetPayload_##n(payloads[(n)]);

        switch (Derived::Count - 1) {
            // clang-format off
            CASE(31);
            CASE(30);
            CASE(29);
            CASE(28);
            CASE(27);
            CASE(26);
            CASE(25);
            CASE(24);
            CASE(23);
            CASE(22);
            CASE(21);
            CASE(20);
            CASE(19);
            CASE(18);
            CASE(17);
            CASE(16);
            CASE(15);
            CASE(14);
            CASE(13);
            CASE(12);
            CASE(11);
            CASE(10);
            CASE(9);
            CASE(8);
            CASE(7);
            CASE(6);
            CASE(5);
            CASE(4);
            CASE(3);
            CASE(2);
            CASE(1);
            CASE(0);
            default:
                break;
        }

#undef CASE
    }
#endif  // DEVICE
};

}  // namespace payloads
}  // namespace device
}  // namespace thesis
