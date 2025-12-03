#include "geometric_validation.h"

#include "thesis/common/geometry/quat.h"

#include <vector_types.h>

using namespace thesis::host::params;
using namespace thesis::common::geometry;

namespace thesis::test::scenes {

// ─────────────────────────────────────────────────────────────────────
// Correctness Tests
// ─────────────────────────────────────────────────────────────────────

TestScene coincident_surfaces() {
    TestScene scene;
    scene.name = "coincident_surfaces";
    scene.description = "Two Gaussians at exact same position (red + blue) → should produce purple";

    // Red Gaussian at origin
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),        // translation
        UnitQuaternion::identity(),           // rotation
        make_float3(0.5f, 0.5f, 0.5f),        // scale
        make_float3(1.0f, 0.0f, 0.0f),        // red albedo
        0.5f                                  // sigma_t
    ));

    // Blue Gaussian at same position
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),        // translation
        UnitQuaternion::identity(),           // rotation
        make_float3(0.5f, 0.5f, 0.5f),        // scale
        make_float3(0.0f, 0.0f, 1.0f),        // blue albedo
        0.5f                                  // sigma_t
    ));

    return scene;
}

TestScene partial_overlap() {
    TestScene scene;
    scene.name = "partial_overlap";
    scene.description = "Two Gaussians partially intersecting → should show red, purple (overlap), blue regions";

    // Red Gaussian on left
    scene.primitives.push_back(Primitive(
        make_float3(-0.3f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(1.0f, 0.0f, 0.0f),        // red
        0.5f
    ));

    // Blue Gaussian on right
    scene.primitives.push_back(Primitive(
        make_float3(0.3f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(0.0f, 0.0f, 1.0f),        // blue
        0.5f
    ));

    return scene;
}

TestScene total_overlap() {
    TestScene scene;
    scene.name = "total_overlap";
    scene.description = "Small Gaussian inside larger one → should show blue core with red halo";

    // Large red Gaussian
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.0f, 1.0f, 1.0f),        // large scale
        make_float3(1.0f, 0.0f, 0.0f),        // red
        0.3f
    ));

    // Small blue Gaussian at center
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.3f, 0.3f, 0.3f),        // small scale
        make_float3(0.0f, 0.0f, 1.0f),        // blue
        0.8f                                  // denser
    ));

    return scene;
}

TestScene depth_ordering() {
    TestScene scene;
    scene.name = "depth_ordering";
    scene.description = "Three Gaussians at different depths → should show correct depth-blended colors";

    // Red Gaussian closest to camera (but in front)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 1.0f),        // closest (in front of camera)
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(1.0f, 0.0f, 0.0f),        // red
        0.5f
    ));

    // Green Gaussian in middle
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 3.0f),        // middle
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(0.0f, 1.0f, 0.0f),        // green
        0.5f
    ));

    // Blue Gaussian farthest
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 5.0f),        // farthest
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(0.0f, 0.0f, 1.0f),        // blue
        0.5f
    ));

    return scene;
}

TestScene camera_inside() {
    TestScene scene;
    scene.name = "camera_inside";
    scene.description = "Camera positioned inside large Gaussian → should show red fog everywhere";

    // Very large red Gaussian encompassing camera
    // Note: Camera is at default position (0, 0, 0) in most configs
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(5.0f, 5.0f, 5.0f),        // very large
        make_float3(1.0f, 0.0f, 0.0f),        // red
        0.2f                                  // less dense for visibility
    ));

    return scene;
}

TestScene non_overlapping() {
    TestScene scene;
    scene.name = "non_overlapping";
    scene.description = "Three separate Gaussians → should show three distinct colored spheres";

    // Red Gaussian left
    scene.primitives.push_back(Primitive(
        make_float3(-1.5f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(1.0f, 0.0f, 0.0f),        // red
        0.5f
    ));

    // Green Gaussian center
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(0.0f, 1.0f, 0.0f),        // green
        0.5f
    ));

    // Blue Gaussian right
    scene.primitives.push_back(Primitive(
        make_float3(1.5f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(0.0f, 0.0f, 1.0f),        // blue
        0.5f
    ));

    return scene;
}

// ─────────────────────────────────────────────────────────────────────
// Transform Tests
// ─────────────────────────────────────────────────────────────────────

TestScene transform_scale() {
    TestScene scene;
    scene.name = "transform_scale";
    scene.description = "Four Gaussians with different scales → validates scale transform";

    // Small isotropic (moved to Z=3.0f to be in front of camera at Z=-1)
    scene.primitives.push_back(Primitive(
        make_float3(-1.0f, 0.5f, 3.0f),
        UnitQuaternion::identity(),
        make_float3(0.2f, 0.2f, 0.2f),        // small uniform
        make_float3(1.0f, 0.0f, 0.0f),
        0.5f
    ));

    // Medium isotropic
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.5f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),        // medium uniform
        make_float3(0.0f, 1.0f, 0.0f),
        0.5f
    ));

    // Large isotropic
    scene.primitives.push_back(Primitive(
        make_float3(1.0f, 0.5f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.0f, 1.0f, 1.0f),        // large uniform
        make_float3(0.0f, 0.0f, 1.0f),
        0.5f
    ));

    // Anisotropic (ellipsoid)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, -0.8f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.0f, 0.3f, 0.3f),        // stretched along X
        make_float3(1.0f, 1.0f, 0.0f),        // yellow
        0.5f
    ));

    return scene;
}

TestScene transform_rotation() {
    TestScene scene;
    scene.name = "transform_rotation";
    scene.description = "Same ellipsoid at different rotations → validates quaternion rotation";

    // Define an elongated ellipsoid
    const auto scale = make_float3(1.0f, 0.3f, 0.3f);  // stretched along X
    constexpr float sigma_t = 0.5f;

    // No rotation (0°)
    scene.primitives.push_back(Primitive(
        make_float3(-1.5f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        scale,
        make_float3(1.0f, 0.0f, 0.0f),        // red
        sigma_t
    ));

    // 45° rotation around Z axis
    // Quaternion for rotation: q = (cos(θ/2), sin(θ/2) * axis)
    // For 45° around Z: θ = π/4, so θ/2 = π/8
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::from_unchecked(
            0.92388f,  // cos(π/8)
            0.0f,
            0.0f,
            0.382683f  // sin(π/8)
        ),
        scale,
        make_float3(0.0f, 1.0f, 0.0f),        // green
        sigma_t
    ));

    // 90° rotation around Z axis
    scene.primitives.push_back(Primitive(
        make_float3(1.5f, 0.0f, 0.0f),
        UnitQuaternion::from_unchecked(
            0.707107f,  // cos(π/4)
            0.0f,
            0.0f,
            0.707107f   // sin(π/4)
        ),
        scale,
        make_float3(0.0f, 0.0f, 1.0f),        // blue
        sigma_t
    ));

    return scene;
}

TestScene transform_translation() { // TODO: doesn't terminate
    TestScene scene;
    scene.name = "transform_translation";
    scene.description = "Same Gaussian at different positions → validates translation";

    const auto rotation = UnitQuaternion::identity();
    const auto scale = make_float3(0.4f, 0.4f, 0.4f);
    const auto albedo = make_float3(0.0f, 1.0f, 1.0f);  // cyan
    constexpr float sigma_t = 0.5f;

    // Restore original 3x3 grid to see if multiple Gaussians cause the hang
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            scene.primitives.push_back(Primitive(
                make_float3(static_cast<float>(x), static_cast<float>(y), 0.0f),
                rotation,
                scale,
                albedo,
                sigma_t
            ));
        }
    }

    return scene;
}

// ─────────────────────────────────────────────────────────────────────
// Debug Tests
// ─────────────────────────────────────────────────────────────────────

TestScene minimal_behind_camera() {
    TestScene scene;
    scene.name = "minimal_behind_camera";
    scene.description = "Single Gaussian behind camera → debug non-termination";

    // Single Gaussian at Z=-2 (camera is at Z=-1 looking forward, so -2 is behind)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, -2.0f),        // Actually behind camera now
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(1.0f, 0.0f, 0.0f),        // red
        0.5f
    ));

    return scene;
}

TestScene minimal_in_front() {
    TestScene scene;
    scene.name = "minimal_in_front";
    scene.description = "Single Gaussian in front of camera → should work";

    // Single Gaussian at Z=3 (in front of camera)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 3.0f),        // In front of camera
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(0.0f, 1.0f, 0.0f),        // green
        0.5f
    ));

    return scene;
}

TestScene multiple_same_z() {
    TestScene scene;
    scene.name = "multiple_same_z";
    scene.description = "Multiple Gaussians at same Z → test for numerical issues";

    // Multiple Gaussians all at Z=2, slightly offset in X/Y
    const auto z = 2.0f;
    const auto scale = make_float3(0.3f, 0.3f, 0.3f);
    constexpr float sigma_t = 0.5f;

    // Create a tight cluster of 9 Gaussians
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            scene.primitives.push_back(Primitive(
                make_float3(static_cast<float>(x) * 0.5f,
                           static_cast<float>(y) * 0.5f,
                           z),  // All at same Z
                UnitQuaternion::identity(),
                scale,
                make_float3(1.0f, 0.0f, 0.0f),  // red
                sigma_t
            ));
        }
    }

    return scene;
}

// ─────────────────────────────────────────────────────────────────────
// Stress Tests
// ─────────────────────────────────────────────────────────────────────

TestScene many_gaussians() {
    TestScene scene;
    scene.name = "many_gaussians";
    scene.description = "100 Gaussians in 10×10 grid → stress test for hit buffer and sorting";

    const auto rotation = UnitQuaternion::identity();
    const auto scale = make_float3(0.1f, 0.1f, 0.1f);  // Smaller to fit more
    constexpr float sigma_t = 0.3f;  // Less dense for better performance

    // 10x10 grid of Gaussians, scaled down to fit camera FOV
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            // Rainbow gradient coloring
            const float hue = static_cast<float>(y * 10 + x) / 100.0f;
            const float r = (hue < 0.5f) ? 1.0f : (1.0f - (hue - 0.5f) * 2.0f);
            const float g = (hue < 0.5f) ? (hue * 2.0f) : 1.0f;
            const float b = (hue < 0.5f) ? 0.0f : ((hue - 0.5f) * 2.0f);

            scene.primitives.push_back(Primitive(
                make_float3(
                    (static_cast<float>(x) - 4.5f) * 0.25f,  // Scale down grid
                    (static_cast<float>(y) - 4.5f) * 0.25f,
                    5.0f                                      // Push forward
                ),
                rotation,
                scale,
                make_float3(r, g, b),
                sigma_t
            ));
        }
    }

    return scene;
}

TestScene nested_structure() {
    TestScene scene;
    scene.name = "nested_structure";
    scene.description = "Three concentric shells → tests complex overlap handling";

    // Outer shell (large, low density, red) - increased density to be visible
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.5f, 1.5f, 1.5f),
        make_float3(1.0f, 0.0f, 0.0f),        // red
        0.25f                                 // increased from 0.15 for visibility
    ));

    // Middle shell (medium, medium density, green)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.0f, 1.0f, 1.0f),
        make_float3(0.0f, 1.0f, 0.0f),        // green
        0.3f
    ));

    // Inner core (small, high density, blue)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(0.0f, 0.0f, 1.0f),        // blue
        0.6f                                  // high density
    ));

    return scene;
}

TestScene tangent_rays() {
    TestScene scene;
    scene.name = "tangent_rays";
    scene.description = "Camera positioned at edge of Gaussian → tests edge case handling";

    // Gaussian positioned such that camera rays will be tangent to surface
    // Camera typically at (0, 0, 0), place Gaussian to the right
    scene.primitives.push_back(Primitive(
        make_float3(1.0f, 0.0f, 0.0f),        // offset to side
        UnitQuaternion::identity(),
        make_float3(0.8f, 0.8f, 0.8f),
        make_float3(1.0f, 1.0f, 0.0f),        // yellow
        0.5f
    ));

    return scene;
}

// ─────────────────────────────────────────────────────────────────────
// Get All Test Scenes
// ─────────────────────────────────────────────────────────────────────

std::vector<TestScene> get_all_test_scenes() {
    return {
        // Debug tests (minimal cases)
        minimal_in_front(),       // Should work
        minimal_behind_camera(),   // Actually in front (Z=0 > -1)
        multiple_same_z(),        // Test multiple overlapping
        // Correctness tests
        coincident_surfaces(),
        partial_overlap(),
        total_overlap(),
        depth_ordering(),
        camera_inside(),
        non_overlapping(),
        // Transform tests
        transform_scale(),
        transform_rotation(),
        transform_translation(),
        // Stress tests
        many_gaussians(),
        nested_structure(),
        tangent_rays()
    };
}

}  // namespace thesis::test::scenes
