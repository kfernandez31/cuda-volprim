#include "thesis/host/utils/io.h"

#include "thesis/pch.h"

#include "thesis/common/geometry/quat.h"
#include "thesis/common/utils/math.h"
#include "thesis/host/utils/result.h"

#include <algorithm>
#include <execution>
#include <miniply/miniply.h>
#include <ranges>

namespace {

using namespace thesis::host::utils;

// One row of the interleaved float buffer extracted from the PLY's "vertex"
// element. Order is fixed by PROP_NAMES below.
constexpr uint32_t NUM_PROPS = 14;
constexpr std::array<const char*, NUM_PROPS> PROP_NAMES = {
    "x",        "y",        "z",                                 // 0..2  : center
    "rot_0",    "rot_1",    "rot_2",    "rot_3",                 // 3..6  : quaternion
    "scale_0",  "scale_1",  "scale_2",                           // 7..9  : log-scale
    "albedo_0", "albedo_1", "albedo_2",                          // 10..12: albedo
    "sigma_t_0",                                                 // 13    : σ_t (linear)
};
constexpr size_t IDX_X = 0, IDX_Y = 1, IDX_Z = 2;
constexpr size_t IDX_ROT0 = 3, IDX_ROT1 = 4, IDX_ROT2 = 5, IDX_ROT3 = 6;
constexpr size_t IDX_SCALE0 = 7, IDX_SCALE1 = 8, IDX_SCALE2 = 9;
constexpr size_t IDX_ALB0 = 10, IDX_ALB1 = 11, IDX_ALB2 = 12;
constexpr size_t IDX_SIGMA = 13;

Result<std::vector<thesis::device::params::Primitive>> loadPrimitivesFromPLY(
    const std::filesystem::path& filename, float sigma_multiplier, float3 albedo_override) {
    miniply::PLYReader reader(filename.string().c_str());
    if (!reader.valid()) {
        return make_error("Failed to open PLY file: {}", filename.string());
    }

    // Walk elements until we find "vertex" — other elements (e.g., "face" in
    // mesh PLYs) are not relevant to per-Gaussian data.
    bool found_vertex = false;
    uint32_t N = 0;
    std::vector<float> buf;

    while (reader.has_element()) {
        if (reader.element_is("vertex")) {
            if (!reader.load_element()) {
                return make_error("Failed to load 'vertex' element from PLY: {}",
                                  filename.string());
            }
            std::array<uint32_t, NUM_PROPS> prop_idxs{};
            for (uint32_t i = 0; i < NUM_PROPS; ++i) {
                prop_idxs[i] = reader.find_property(PROP_NAMES[i]);
                if (prop_idxs[i] == miniply::kInvalidIndex) {
                    return make_error("PLY file '{}' missing required property '{}'",
                                      filename.string(), PROP_NAMES[i]);
                }
            }

            N = reader.num_rows();
            // One contiguous N×NUM_PROPS row-major float buffer. miniply picks
            // the optimal extraction path internally (memcpy if source columns
            // are already contiguous, conversion loop otherwise).
            buf.resize(static_cast<size_t>(N) * NUM_PROPS);
            if (!reader.extract_properties(prop_idxs.data(), NUM_PROPS,
                                           miniply::PLYPropertyType::Float, buf.data())) {
                return make_error("Failed to extract vertex properties from PLY: {}",
                                  filename.string());
            }
            found_vertex = true;
            break;
        }
        reader.next_element();
    }

    if (!found_vertex) {
        return make_error("PLY file '{}' has no 'vertex' element", filename.string());
    }

    using namespace thesis::host;
    using namespace thesis::common::geometry;
    using Primitive = thesis::device::params::Primitive;

    std::vector<Primitive> result(N, Primitive{});

    // Hoisted: same answer for every primitive, so don't re-evaluate per-iter.
    const bool use_albedo_override = albedo_override.x >= 0.0f &&
                                     albedo_override.y >= 0.0f &&
                                     albedo_override.z >= 0.0f;

    auto indices = std::views::iota(uint32_t{0}, N);

    // Phase 1: Parallel construction (expf, quaternion math, Primitive precomputation).
    // Each row of `buf` is contiguous, so per-primitive reads are cache-friendly.
    std::for_each(std::execution::par, indices.begin(), indices.end(), [&](uint32_t i) {
        const float* row = buf.data() + static_cast<size_t>(i) * NUM_PROPS;
        auto center = make_float3(row[IDX_X], row[IDX_Y], row[IDX_Z]);
        auto quat = UnitQuaternion::from(row[IDX_ROT0], row[IDX_ROT1], row[IDX_ROT2],
                                         row[IDX_ROT3]);
        auto scale = make_float3(expf(row[IDX_SCALE0]), expf(row[IDX_SCALE1]),
                                 expf(row[IDX_SCALE2]));
        auto albedo = use_albedo_override
                          ? albedo_override
                          : make_float3(row[IDX_ALB0], row[IDX_ALB1], row[IDX_ALB2]);
        // Match Mitsuba's volprim_tomography convention (volumetric_primitives/
        // volprim/integrators/common.py::GaussianKernel.density_integral):
        // sigma_t in the PLY is the per-primitive *total integrated mass* M.
        // The kernel evaluates τ(ray) = M · density_integral(ray) where
        // density_integral already accounts for the scale via 1/(2π · sqrt(...)).
        // No (2π)^{3/2}·∏s bridge — the PLY values were trained to Mitsuba's
        // convention, so applying the bridge would scale them by the wrong factor
        // and our render would never reach Mitsuba's optical density at the same
        // numerical sigma_multiplier.
        auto optical_thickness = row[IDX_SIGMA] * sigma_multiplier;

        result[i] = Primitive::from_forward_quat(center, quat, scale, albedo, optical_thickness);
    });

    // Phase 2a: Parallel albedo clamping. Pure warn-and-fix, no early exit, so
    // it parallelizes cleanly. spdlog default sinks are thread-safe (mt sinks).
    std::for_each(std::execution::par, indices.begin(), indices.end(), [&](uint32_t i) {
        auto& albedo = result[i].albedo_;
        auto clamp_albedo = [i](float& val, const char* component) {
            if (!std::isfinite(val)) {
                spdlog::warn("Primitive {}: NaN/Inf in albedo.{}, setting to 0", i, component);
                val = 0.0f;
            } else if (val < 0.0f) {
                spdlog::warn("Primitive {}: Negative albedo.{} = {}, clamping to 0", i, component,
                             val);
                val = 0.0f;
            } else if (val > 1.0f) {
                spdlog::warn("Primitive {}: albedo.{} = {} > 1.0, clamping to 1.0", i, component,
                             val);
                val = 1.0f;
            }
        };
        clamp_albedo(albedo.x, "r");
        clamp_albedo(albedo.y, "g");
        clamp_albedo(albedo.z, "b");
    });

    // Phase 2b: Sequential validation (early exit on error — keeps the first
    // failing primitive's index in the error message and stops scanning).
    for (uint32_t i = 0; i < N; ++i) {
        const auto scale = result[i].scale();
        const auto center = result[i].center();
        const auto optical_thickness = result[i].optical_thickness_;

        if (scale.x <= 0.0f || scale.y <= 0.0f || scale.z <= 0.0f) {
            return make_error(
                "Primitive {}: Invalid scale ({}, {}, {}) - all components must be > 0", i,
                scale.x, scale.y, scale.z);
        }
        if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z)) {
            return make_error("Primitive {}: NaN/Inf in scale ({}, {}, {})", i, scale.x, scale.y,
                              scale.z);
        }
        if (optical_thickness <= 0.0f) {
            return make_error("Primitive {}: Invalid optical_thickness {} - must be > 0", i,
                              optical_thickness);
        }
        if (!std::isfinite(optical_thickness)) {
            return make_error("Primitive {}: NaN/Inf in optical_thickness {}", i,
                              optical_thickness);
        }
        if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) {
            return make_error("Primitive {}: NaN/Inf in center ({}, {}, {})", i, center.x,
                              center.y, center.z);
        }
    }

    return result;
}

}  // namespace

namespace thesis::host::utils::io::async {

using Primitive = thesis::device::params::Primitive;

std::future<Result<std::vector<Primitive>>> loadPrimitives(const std::filesystem::path& filename,
                                                           float sigma_multiplier,
                                                           float3 albedo_override) {
    return std::async(
        std::launch::async,
        [filename, sigma_multiplier, albedo_override]() -> Result<std::vector<Primitive>> {
            return loadPrimitivesFromPLY(filename, sigma_multiplier, albedo_override);
        });
}

}  // namespace thesis::host::utils::io::async
