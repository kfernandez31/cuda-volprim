#include "single_gaussian.h"

#include "thesis/common/geometry/quat.h"
#include "thesis/common/utils/math.h"

#include <spdlog/spdlog.h>

namespace thesis::test::scenes {

using namespace thesis::host::utils;

namespace {

constexpr size_t SINGLE_GAUSSIAN_WIDTH = 256;
constexpr size_t SINGLE_GAUSSIAN_HEIGHT = 256;
// Isotropic Gaussian scale. Camera framing is derived proportionally so the
// viewport always spans ±3σ regardless of scale. 1.0 = classic unit test
// (matches tools/refs/single_gaussian_analytic.py, which assumes scale=1).
// Set to a realistic cloud scale (~0.047) to exercise the scale-normalization
// term the unit test is blind to.
constexpr float SINGLE_GAUSSIAN_SCALE = 1.0f;
constexpr float SINGLE_GAUSSIAN_ORTHO_HEIGHT = 6.0f * SINGLE_GAUSSIAN_SCALE;   // ±3σ
constexpr float SINGLE_GAUSSIAN_CAMERA_DISTANCE = 5.0f * SINGLE_GAUSSIAN_SCALE;

}  // namespace

Result<MultiViewTestScene> single_gaussian_validation(float sigma_multiplier) {
    using Primitive = thesis::device::params::Primitive;
    using thesis::common::geometry::UnitQuaternion;
    namespace math = thesis::common::math;

    MultiViewTestScene scene;
    scene.name = "single_gaussian_validation";
    scene.description =
        "Single isotropic absorber Gaussian at origin, ortho view in [-3,3]^2 — "
        "closed-form analytic verification of escape transmittance";

    // One isotropic primitive at origin, scale (1, 1, 1), albedo = 0.
    // optical_thickness IS the per-primitive total integrated mass M, in
    // Mitsuba's volprim_tomography convention (see src/.../io/ply.cpp).
    // No (2π)^{3/2}·∏s bridge — see ply.cpp comment for why.
    const auto center = make_float3(0.0f, 0.0f, 0.0f);
    const auto scale = make_float3(SINGLE_GAUSSIAN_SCALE, SINGLE_GAUSSIAN_SCALE,
                                   SINGLE_GAUSSIAN_SCALE);
    const auto albedo = make_float3(0.0f, 0.0f, 0.0f);
    const auto optical_thickness = sigma_multiplier;

    scene.primitives.push_back(Primitive::from_forward_quat(
        center, UnitQuaternion::identity(), scale, albedo, optical_thickness));

    // Orthographic camera looking along +Z from z = -5. Viewport spans
    // [-3, 3] x [-3, 3] world units (3σ envelope fits cleanly).
    const auto cam_origin =
        make_float3(0.0f, 0.0f, -SINGLE_GAUSSIAN_CAMERA_DISTANCE);
    const auto cam_target = make_float3(0.0f, 0.0f, 0.0f);
    const auto cam_up = make_float3(0.0f, 1.0f, 0.0f);

    auto camera = thesis::host::params::Camera::createOrthographic(
        SINGLE_GAUSSIAN_WIDTH, SINGLE_GAUSSIAN_HEIGHT, cam_origin, cam_target, cam_up,
        SINGLE_GAUSSIAN_ORTHO_HEIGHT);

    scene.cameras.push_back({camera, SINGLE_GAUSSIAN_WIDTH, SINGLE_GAUSSIAN_HEIGHT});

    scene.env_map_override = "assets/white_constant.hdr";

    spdlog::info("single_gaussian_validation: sigma_multiplier={}, optical_thickness={:.4f}",
                 sigma_multiplier, optical_thickness);

    return scene;
}

}  // namespace thesis::test::scenes
