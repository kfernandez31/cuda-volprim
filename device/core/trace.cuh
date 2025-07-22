#pragma once

#include "thesis/device/geometry/ray.h"
#include "thesis/common/utils/types.h"
#include "thesis/device/utils/result.h"
#include "thesis/device/payloads/base.h"
#include "thesis/device/payloads/closesthit.h"

#include <optix.h>
#include <vector_types.h>

// TODO(kacper): think of optixReorder

#define TRACE_PAYLOADS_0(p) p[0]
#define TRACE_PAYLOADS_1(p) TRACE_PAYLOADS_0(p), p[1]
#define TRACE_PAYLOADS_2(p) TRACE_PAYLOADS_1(p), p[2]
#define TRACE_PAYLOADS_3(p) TRACE_PAYLOADS_2(p), p[3]
#define TRACE_PAYLOADS_4(p) TRACE_PAYLOADS_3(p), p[4]
#define TRACE_PAYLOADS_5(p) TRACE_PAYLOADS_4(p), p[5]
#define TRACE_PAYLOADS_6(p) TRACE_PAYLOADS_5(p), p[6]
#define TRACE_PAYLOADS_7(p) TRACE_PAYLOADS_6(p), p[7]
#define TRACE_PAYLOADS_8(p) TRACE_PAYLOADS_7(p), p[8]
#define TRACE_PAYLOADS_9(p) TRACE_PAYLOADS_8(p), p[9]
#define TRACE_PAYLOADS_10(p) TRACE_PAYLOADS_9(p), p[10]
#define TRACE_PAYLOADS_11(p) TRACE_PAYLOADS_10(p), p[11]
#define TRACE_PAYLOADS_12(p) TRACE_PAYLOADS_11(p), p[12]
#define TRACE_PAYLOADS_13(p) TRACE_PAYLOADS_12(p), p[13]
#define TRACE_PAYLOADS_14(p) TRACE_PAYLOADS_13(p), p[14]
#define TRACE_PAYLOADS_15(p) TRACE_PAYLOADS_14(p), p[15]
#define TRACE_PAYLOADS_16(p) TRACE_PAYLOADS_15(p), p[16]
#define TRACE_PAYLOADS_17(p) TRACE_PAYLOADS_16(p), p[17]
#define TRACE_PAYLOADS_18(p) TRACE_PAYLOADS_17(p), p[18]
#define TRACE_PAYLOADS_19(p) TRACE_PAYLOADS_18(p), p[19]
#define TRACE_PAYLOADS_20(p) TRACE_PAYLOADS_19(p), p[20]
#define TRACE_PAYLOADS_21(p) TRACE_PAYLOADS_20(p), p[21]
#define TRACE_PAYLOADS_22(p) TRACE_PAYLOADS_21(p), p[22]
#define TRACE_PAYLOADS_23(p) TRACE_PAYLOADS_22(p), p[23]
#define TRACE_PAYLOADS_24(p) TRACE_PAYLOADS_23(p), p[24]
#define TRACE_PAYLOADS_25(p) TRACE_PAYLOADS_24(p), p[25]
#define TRACE_PAYLOADS_26(p) TRACE_PAYLOADS_25(p), p[26]
#define TRACE_PAYLOADS_27(p) TRACE_PAYLOADS_26(p), p[27]
#define TRACE_PAYLOADS_28(p) TRACE_PAYLOADS_27(p), p[28]
#define TRACE_PAYLOADS_29(p) TRACE_PAYLOADS_28(p), p[29]
#define TRACE_PAYLOADS_30(p) TRACE_PAYLOADS_29(p), p[30]
#define TRACE_PAYLOADS_31(p) TRACE_PAYLOADS_30(p), p[31]

namespace thesis {
namespace device {
namespace consts {
constexpr float INF_F = 1e20f;
constexpr unsigned VISIBILITY_ALL = 0xFFu;
constexpr float INTERSECTION_EPS = 1e-3f;
} // namespace consts

// TODO(kacper): consider optixUndefinedValue()
template <uint FLAGS>
__device__ __forceinline__ auto trace_impl(const geometry::Ray& ray, float t_min, float eps=consts::INTERSECTION_EPS) {
    uint ps[payloads::MAX_PAYLOADS] = {};

    optixTrace(
        launch_params.gas_handle_,
        ray.origin_,
        ray.direction_,
        t_min + eps,       // Min intersection distance
        consts::INF_F,                 // Max intersection distance
        0.0f,                  // Disable motion blur
        consts::VISIBILITY_ALL,
        FLAGS,
        geometry::Ray::Type::RADIANCE,   // SBT offset
        geometry::Ray::Type::COUNT,      // SBT stride
        0,                     // miss SBT index: first miss program
        TRACE_PAYLOADS_3(ps)
    );

    const auto tag = static_cast<payloads::Tag>(ps[0]);
    utils::Result<payloads::ClosestHit, payloads::Miss> result;

    if (tag == payloads::Tag::ClosestHit) {
        payloads::ClosestHit hit;
        hit.unpack(ps);
        result.emplace_ok(hit);
    } else {
        payloads::Miss miss;
        miss.unpack(ps);
        result.emplace_err(miss);
    }

    return result;
}

__device__ __forceinline__ auto trace_ch(const geometry::Ray& ray, float t_min) {
    return trace_impl<OPTIX_RAY_FLAG_DISABLE_ANYHIT>(ray, t_min);
}

__device__ __forceinline__ auto trace_ah(const geometry::Ray& ray, float t_min) {
    return trace_impl<OPTIX_RAY_FLAG_ENFORCE_ANYHIT | OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT>(ray, t_min);
}

} // namespace device
} // namespace thesis
