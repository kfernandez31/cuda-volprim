#include "single_gaussian.h"

#include "thesis/common/geometry/quat.h"
#include "thesis/common/utils/math.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <cmath>

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

    // One primitive at origin, albedo = 0.
    // optical_thickness IS the per-primitive total integrated mass M, in
    // Mitsuba's volprim_tomography convention (see src/.../io/ply.cpp).
    // No (2π)^{3/2}·∏s bridge — see ply.cpp comment for why.
    const auto center = make_float3(0.0f, 0.0f, 0.0f);
    const auto albedo = make_float3(0.0f, 0.0f, 0.0f);
    const auto optical_thickness = sigma_multiplier;

    // SG_TRANSFORMED=1 exercises the anisotropic-scale + rotation whitening path
    // (the transform math the isotropic identity test is blind to). Config is
    // mirrored EXACTLY in tools/refs/render_single_gaussian_via_prb.py:
    //   scale = (1.0, 0.5, 0.75)  (max σ=1 → 3σ envelope still fits the ±3 viewport)
    //   forward rotation = 30° about +Z  (tilts the XY ellipse on screen; a
    //   handedness mismatch would be visually obvious, not silent).
    const char* sg_transformed = std::getenv("SG_TRANSFORMED");
    const bool transformed = sg_transformed && std::string_view(sg_transformed) == "1";

    float3 scale;
    UnitQuaternion forward_quat;
    if (transformed) {
        scale = make_float3(1.0f, 0.5f, 0.75f);
        // Forward (local→world) rotation 30° about +Z: w=cos(15°), z=sin(15°).
        constexpr float half = 0.5f * 30.0f * math::PI_F / 180.0f;
        forward_quat = UnitQuaternion::from(std::cos(half), 0.0f, 0.0f, std::sin(half));
    } else {
        scale = make_float3(SINGLE_GAUSSIAN_SCALE, SINGLE_GAUSSIAN_SCALE,
                            SINGLE_GAUSSIAN_SCALE);
        forward_quat = UnitQuaternion::identity();
    }
    scene.description = transformed
        ? "Single ANISOTROPIC+ROTATED absorber Gaussian at origin (scale=(1,0.5,0.75), "
          "30° about +Z), ortho view in [-3,3]^2 — whitening-transform verification"
        : scene.description;

    scene.primitives.push_back(Primitive::from_forward_quat(
        center, forward_quat, scale, albedo, optical_thickness));

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

Result<MultiViewTestScene> two_gaussian_validation(float sigma_multiplier) {
    using Primitive = thesis::device::params::Primitive;
    using thesis::common::geometry::UnitQuaternion;

    MultiViewTestScene scene;
    scene.name = "two_gaussian_validation";
    scene.description =
        "Two isotropic absorber Gaussians at distinct positions/depths — minimal "
        "per-ray distinct-position accumulation test";

    const auto scale = make_float3(1.0f, 1.0f, 1.0f);
    const auto albedo = make_float3(0.0f, 0.0f, 0.0f);
    const auto optical_thickness = sigma_multiplier;

    // Two Gaussians offset in x (distinct perpendicular distance per pixel) AND in z
    // (distinct entry t_hit along each ray). Offsets < 1σ so their footprints overlap
    // in screen space → many rays pierce both.
    scene.primitives.push_back(Primitive::from_forward_quat(
        make_float3(-0.5f, 0.0f, -0.5f), UnitQuaternion::identity(), scale, albedo,
        optical_thickness));
    scene.primitives.push_back(Primitive::from_forward_quat(
        make_float3(0.5f, 0.0f, 0.5f), UnitQuaternion::identity(), scale, albedo,
        optical_thickness));

    const auto cam_origin = make_float3(0.0f, 0.0f, -SINGLE_GAUSSIAN_CAMERA_DISTANCE);
    const auto cam_target = make_float3(0.0f, 0.0f, 0.0f);
    const auto cam_up = make_float3(0.0f, 1.0f, 0.0f);

    auto camera = thesis::host::params::Camera::createOrthographic(
        SINGLE_GAUSSIAN_WIDTH, SINGLE_GAUSSIAN_HEIGHT, cam_origin, cam_target, cam_up,
        SINGLE_GAUSSIAN_ORTHO_HEIGHT);

    scene.cameras.push_back({camera, SINGLE_GAUSSIAN_WIDTH, SINGLE_GAUSSIAN_HEIGHT});
    scene.env_map_override = "assets/white_constant.hdr";

    spdlog::info("two_gaussian_validation: sigma_multiplier={}, optical_thickness={:.4f}",
                 sigma_multiplier, optical_thickness);

    return scene;
}

// ─────────────────────────────────────────────────────────────────────────────
// Procedural multi-primitive clusters for climbing the overlap ladder toward the
// cloud. Driven by env SG_CLUSTER_MODE so a single binary + a single Mitsuba
// script (tools/refs/render_cluster_via_prb.py) stay convention-locked. ALL
// layouts are DETERMINISTIC closed-form (no RNG) so the two sides match exactly.
//   n5     : 5 isotropic overlapping absorbers (distinct positions+depths)
//   stress : K collinear-in-z absorbers (env SG_STRESS_K) — forces high
//            simultaneous overlap to cross MAX_ACTIVE_PRIMS=64 / HIT_BUFFER=128.
//   traits : anisotropic + rotated + small-scale + per-prim-varied-sigma cluster.
// ─────────────────────────────────────────────────────────────────────────────
Result<MultiViewTestScene> cluster_validation(float /*sigma_multiplier*/) {
    using Primitive = thesis::device::params::Primitive;
    using thesis::common::geometry::UnitQuaternion;
    namespace math = thesis::common::math;

    const char* mode_env = std::getenv("SG_CLUSTER_MODE");
    const std::string mode = mode_env ? mode_env : "n5";

    MultiViewTestScene scene;
    scene.name = "cluster_validation";
    scene.description = "Procedural overlap cluster (mode=" + mode + ")";

    const auto albedo = make_float3(0.0f, 0.0f, 0.0f);
    const auto zrot = [](float deg) {
        const float h = 0.5f * deg * math::PI_F / 180.0f;
        return UnitQuaternion::from(std::cos(h), 0.0f, 0.0f, std::sin(h));
    };

    float ortho_height = 6.0f;
    const float camera_distance = 6.0f;

    if (mode == "n5") {
        ortho_height = 8.0f;  // ±4
        const float M = 2.0f;
        const float3 P[5] = {
            make_float3(0.0f, 0.0f, 0.0f),
            make_float3(0.5f, 0.0f, 0.3f),
            make_float3(-0.5f, 0.0f, -0.3f),
            make_float3(0.0f, 0.5f, 0.4f),
            make_float3(0.0f, -0.5f, -0.4f),
        };
        for (const auto& c : P) {
            scene.primitives.push_back(Primitive::from_forward_quat(
                c, UnitQuaternion::identity(), make_float3(1.0f, 1.0f, 1.0f), albedo, M));
        }
    } else if (mode == "stress") {
        ortho_height = 6.0f;  // ±3
        const char* k_env = std::getenv("SG_STRESS_K");
        const int K = k_env ? std::atoi(k_env) : 80;
        const float Z0 = 1.5f;            // z-span [-1.5, 1.5]
        const float total_mass = 10.0f;   // split across K → discriminating core T~0.2
        const float M = total_mass / static_cast<float>(K);
        for (int k = 0; k < K; ++k) {
            const float z = (K == 1) ? 0.0f
                                     : -Z0 + 2.0f * Z0 * static_cast<float>(k) /
                                                 static_cast<float>(K - 1);
            scene.primitives.push_back(Primitive::from_forward_quat(
                make_float3(0.0f, 0.0f, z), UnitQuaternion::identity(),
                make_float3(1.0f, 1.0f, 1.0f), albedo, M));
        }
        spdlog::info("cluster_validation stress: K={}, M_per_prim={:.5f}", K, M);
    } else if (mode == "traits") {
        ortho_height = 4.0f;  // ±2
        struct Spec { float3 c; float3 s; float zrot_deg; float M; };
        const Spec specs[8] = {
            {make_float3( 0.00f, 0.00f, 0.00f), make_float3(0.50f, 0.30f, 0.40f),   0.0f, 2.0f},
            {make_float3( 0.40f, 0.10f, 0.20f), make_float3(0.40f, 0.50f, 0.30f),  30.0f, 1.5f},
            {make_float3(-0.35f, 0.20f,-0.20f), make_float3(0.30f, 0.40f, 0.50f), -45.0f, 2.5f},
            {make_float3( 0.10f,-0.40f, 0.30f), make_float3(0.50f, 0.35f, 0.30f),  60.0f, 1.8f},
            {make_float3(-0.20f,-0.30f,-0.30f), make_float3(0.45f, 0.30f, 0.40f), -20.0f, 2.2f},
            {make_float3( 0.30f, 0.35f,-0.25f), make_float3(0.35f, 0.45f, 0.30f),  15.0f, 1.6f},
            {make_float3(-0.40f,-0.10f, 0.35f), make_float3(0.40f, 0.40f, 0.50f),  80.0f, 2.0f},
            {make_float3( 0.15f, 0.40f, 0.10f), make_float3(0.30f, 0.50f, 0.35f), -60.0f, 1.9f},
        };
        for (const auto& sp : specs) {
            scene.primitives.push_back(Primitive::from_forward_quat(
                sp.c, zrot(sp.zrot_deg), sp.s, albedo, sp.M));
        }
    } else {
        return thesis::host::utils::make_error("unknown SG_CLUSTER_MODE: " + mode);
    }

    const auto cam_origin = make_float3(0.0f, 0.0f, -camera_distance);
    const auto cam_target = make_float3(0.0f, 0.0f, 0.0f);
    const auto cam_up = make_float3(0.0f, 1.0f, 0.0f);
    auto camera = thesis::host::params::Camera::createOrthographic(
        SINGLE_GAUSSIAN_WIDTH, SINGLE_GAUSSIAN_HEIGHT, cam_origin, cam_target, cam_up,
        ortho_height);
    scene.cameras.push_back({camera, SINGLE_GAUSSIAN_WIDTH, SINGLE_GAUSSIAN_HEIGHT});
    scene.env_map_override = "assets/white_constant.hdr";

    spdlog::info("cluster_validation: mode={}, n_prims={}, ortho_height={}",
                 mode, scene.primitives.size(), ortho_height);
    return scene;
}

}  // namespace thesis::test::scenes
