#include "single_gaussian.h"

#include "thesis/common/geometry/quat.h"
#include "thesis/common/utils/math.h"

#include <spdlog/spdlog.h>

namespace thesis::test::scenes {

using namespace thesis::host::utils;

namespace {

constexpr size_t SINGLE_GAUSSIAN_WIDTH = 256;
constexpr size_t SINGLE_GAUSSIAN_HEIGHT = 256;
constexpr float SINGLE_GAUSSIAN_ORTHO_HEIGHT = 6.0f;  // viewport [-3, 3] in y
constexpr float SINGLE_GAUSSIAN_CAMERA_DISTANCE = 5.0f;  // camera at z = -5

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
    // Peak extinction = sigma_multiplier; bridge to mass-normalised optical_thickness
    // matches the convention used everywhere else (see src/.../io/ply.cpp).
    const auto center = make_float3(0.0f, 0.0f, 0.0f);
    const auto scale = make_float3(1.0f, 1.0f, 1.0f);
    const auto albedo = make_float3(0.0f, 0.0f, 0.0f);
    const auto optical_thickness =
        sigma_multiplier * math::TWO_PI_POW_3_2_F * scale.x * scale.y * scale.z;

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
