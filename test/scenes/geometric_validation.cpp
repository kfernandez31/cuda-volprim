#include "geometric_validation.h"

#include "thesis/common/geometry/quat.h"

#include <vector_types.h>

using namespace thesis::host::params;
using namespace thesis::common::geometry;

namespace thesis::test::scenes {

// ─────────────────────────────────────────────────────────────────────
// Correctness Tests
// ─────────────────────────────────────────────────────────────────────

TestScene single_purple() {
    TestScene scene;
    scene.name = "single_purple";
    scene.description = "Single purple Gaussian → reference for coincident_surfaces test";

    // Purple Gaussian (reference for red+blue overlap)
    // Albedo: density-weighted average = (1,0,0)*0.5 + (0,0,1)*0.5 = (0.5, 0, 0.5)
    // Sigma_t: sum of optical thicknesses = 0.5 + 0.5 = 1.0
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),        // translation
        UnitQuaternion::identity(),           // rotation
        make_float3(0.5f, 0.5f, 0.5f),        // scale
        make_float3(0.5f, 0.0f, 0.5f),        // purple albedo (density-weighted average)
        1.0f                                  // sigma_t (sum of both)
    ));

    return scene;
}

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
    scene.description = "Two Gaussians partially intersecting → should show blue, purple (overlap), red regions [COLORS SWAPPED FOR DEBUG]";

    // Blue Gaussian on left (SWAPPED)
    scene.primitives.push_back(Primitive(
        make_float3(-0.3f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(0.0f, 0.0f, 1.0f),        // blue
        0.5f
    ));

    // Red Gaussian on right (SWAPPED)
    scene.primitives.push_back(Primitive(
        make_float3(0.3f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(1.0f, 0.0f, 0.0f),        // red
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

TestScene transform_translation() {
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
// Performance Stress Tests (Increasing Gaussian Counts)
// All gaussians positioned to be visible in camera frustum
// ─────────────────────────────────────────────────────────────────────

namespace {

// Helper function to create stress test grids with gradient coloring
TestScene create_grid_stress_test(
    const std::string& name,
    const std::string& description,
    int grid_x,
    int grid_y,
    float spacing,
    float3 scale,
    float z_depth = 5.0f,
    float sigma_t = 0.5f
) {
    TestScene scene;
    scene.name = name;
    scene.description = description;

    const float grid_width = (grid_x - 1) * spacing;
    const float grid_height = (grid_y - 1) * spacing;
    const float offset_x = -grid_width * 0.5f;
    const float offset_y = -grid_height * 0.5f;

    for (int y = 0; y < grid_y; ++y) {
        for (int x = 0; x < grid_x; ++x) {
            const float r = static_cast<float>(x) / (grid_x - 1);
            const float b = static_cast<float>(y) / (grid_y - 1);
            const auto albedo = make_float3(r, 0.5f, b);

            scene.primitives.push_back(Primitive(
                make_float3(offset_x + x * spacing, offset_y + y * spacing, z_depth),
                UnitQuaternion::identity(),
                scale,
                albedo,
                sigma_t
            ));
        }
    }

    return scene;
}

}  // anonymous namespace

TestScene stress_256_gaussians() {
    return create_grid_stress_test(
        "stress_256_gaussians",
        "16×16 grid (256 gaussians) in front of camera → performance baseline",
        16, 16,  // grid_x, grid_y
        0.4f,  // spacing
        make_float3(0.2f, 0.2f, 0.2f)  // scale
    );
}

TestScene stress_512_gaussians() {
    return create_grid_stress_test(
        "stress_512_gaussians",
        "16×32 grid (512 gaussians) → moderate density",
        32, 16,  // grid_x, grid_y
        0.2f,  // spacing
        make_float3(0.1f, 0.1f, 0.1f)  // scale
    );
}

TestScene stress_1024_gaussians() {
    return create_grid_stress_test(
        "stress_1024_gaussians",
        "32×32 grid (1024 gaussians) → heavy BVH traversal",
        32, 32,  // grid_x, grid_y
        0.2f,  // spacing
        make_float3(0.1f, 0.1f, 0.1f)  // scale
    );
}

TestScene stress_2048_gaussians() {
    return create_grid_stress_test(
        "stress_2048_gaussians",
        "32×64 grid (2048 gaussians) → extreme density",
        64, 32,  // grid_x, grid_y
        0.1f,  // spacing
        make_float3(0.05f, 0.05f, 0.05f)  // scale
    );
}

TestScene stress_4096_gaussians() {
    return create_grid_stress_test(
        "stress_4096_gaussians",
        "64×64 grid (4096 gaussians) → maximum stress test",
        64, 64,  // grid_x, grid_y
        0.1f,  // spacing
        make_float3(0.05f, 0.05f, 0.05f)  // scale
    );
}

TestScene stress_8192_gaussians() {
    return create_grid_stress_test(
        "stress_8192_gaussians",
        "64×128 grid (8192 gaussians) → beyond maximum stress",
        128, 64,  // grid_x, grid_y
        0.05f,  // spacing
        make_float3(0.025f, 0.025f, 0.025f)  // scale
    );
}

// ─────────────────────────────────────────────────────────────────────
// Edge Case Tests: Priority 0 (Critical - Could Crash/Hang)
// ─────────────────────────────────────────────────────────────────────

TestScene empty_scene() {
    TestScene scene;
    scene.name = "empty_scene";
    scene.description = "No primitives at all → should show only environment map";

    // Intentionally empty - no primitives added
    // Tests null pointer/empty buffer handling

    return scene;
}

TestScene hit_buffer_at_capacity() {
    TestScene scene;
    scene.name = "hit_buffer_at_capacity";
    scene.description = "Exactly 64 Gaussians in line → tests MAX_PRIMITIVES boundary (128 hits = HIT_BUFFER_CAPACITY)";

    // Create tunnel of overlapping Gaussians along Z-axis
    // Each has radius ~0.3, spaced 0.5 apart for guaranteed hits
    // Low density to prevent early path termination
    // 64 primitives × 2 hits (entry + exit) = 128 hits (exactly at capacity)
    for (int i = 0; i < 64; ++i) {
        scene.primitives.push_back(Primitive(
            make_float3(0.0f, 0.0f, static_cast<float>(i) * 0.5f + 1.0f),
            UnitQuaternion::identity(),
            make_float3(0.3f, 0.3f, 0.3f),
            make_float3(1.0f, 0.0f, 0.0f),  // red
            0.05f  // very low density
        ));
    }

    return scene;
}

TestScene hit_buffer_overflow() {
    TestScene scene;
    scene.name = "hit_buffer_overflow";
    scene.description = "65 Gaussians in line → tests overflow handling (130 hits > HIT_BUFFER_CAPACITY)";

    // 65 primitives × 2 hits (entry + exit) = 130 hits (exceeds capacity by 2)
    // Tests that overflow is handled gracefully (ray termination in anyhit, no crash)
    for (int i = 0; i < 65; ++i) {
        scene.primitives.push_back(Primitive(
            make_float3(0.0f, 0.0f, static_cast<float>(i) * 0.5f + 1.0f),
            UnitQuaternion::identity(),
            make_float3(0.3f, 0.3f, 0.3f),
            make_float3(0.0f, 1.0f, 0.0f),  // green
            0.05f
        ));
    }

    return scene;
}


TestScene ray_at_exact_boundary() {
    TestScene scene;
    scene.name = "ray_at_exact_boundary";
    scene.description = "Camera positioned exactly at Gaussian surface → tests t≈0 entry detection";

    // Camera is at (0, 0, -1) by default
    // Place Gaussian such that camera is exactly at surface
    // Gaussian at (0, 0, 0) with scale 1.0 means surface at distance 1.0 from center
    // So camera at Z=-1 is exactly at the back surface
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.0f, 1.0f, 1.0f),
        make_float3(1.0f, 1.0f, 0.0f),  // yellow
        0.3f
    ));

    return scene;
}

TestScene all_behind_camera() {
    TestScene scene;
    scene.name = "all_behind_camera";
    scene.description = "All primitives behind camera → should show only environment map";

    // Camera at (0, 0, -1) looking forward (+Z direction)
    // Place all Gaussians at negative Z (behind camera)
    for (int i = 0; i < 5; ++i) {
        scene.primitives.push_back(Primitive(
            make_float3(
                static_cast<float>(i - 2),
                0.0f,
                -5.0f - static_cast<float>(i)  // All at Z < -1 (behind camera)
            ),
            UnitQuaternion::identity(),
            make_float3(0.5f, 0.5f, 0.5f),
            make_float3(1.0f, 0.0f, 1.0f),  // magenta
            0.5f
        ));
    }

    return scene;
}

// ─────────────────────────────────────────────────────────────────────
// Edge Case Tests: Priority 1 (Numerical Stability)
// ─────────────────────────────────────────────────────────────────────

TestScene extreme_anisotropic_scale() {
    TestScene scene;
    scene.name = "extreme_anisotropic_scale";
    scene.description = "Extreme 100:1 scale ratios → tests numerical stability with needle/pancake shapes";

    // Needle shape (stretched along X)
    scene.primitives.push_back(Primitive(
        make_float3(-1.0f, 0.5f, 3.0f),
        UnitQuaternion::identity(),
        make_float3(5.0f, 0.05f, 0.05f),  // 100:1 ratio
        make_float3(1.0f, 0.0f, 0.0f),  // red
        0.3f
    ));

    // Pancake shape (flattened along Y)
    scene.primitives.push_back(Primitive(
        make_float3(1.0f, 0.5f, 3.0f),
        UnitQuaternion::identity(),
        make_float3(1.0f, 0.01f, 1.0f),  // 100:1 ratio
        make_float3(0.0f, 0.0f, 1.0f),  // blue
        0.3f
    ));

    // Needle along Z
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, -0.5f, 3.0f),
        UnitQuaternion::identity(),
        make_float3(0.05f, 0.05f, 5.0f),  // 100:1 ratio
        make_float3(0.0f, 1.0f, 0.0f),  // green
        0.3f
    ));

    return scene;
}

TestScene extreme_small_scale() {
    TestScene scene;
    scene.name = "extreme_small_scale";
    scene.description = "Very tiny Gaussians (0.01 units) → tests precision limits";

    // Create grid of tiny Gaussians
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            scene.primitives.push_back(Primitive(
                make_float3(
                    static_cast<float>(x) * 0.1f,
                    static_cast<float>(y) * 0.1f,
                    2.0f
                ),
                UnitQuaternion::identity(),
                make_float3(0.01f, 0.01f, 0.01f),  // extremely small
                make_float3(1.0f, 1.0f, 0.0f),  // yellow
                1.0f  // high density so they're visible
            ));
        }
    }

    return scene;
}

TestScene extreme_large_scale() {
    TestScene scene;
    scene.name = "extreme_large_scale";
    scene.description = "Very large Gaussians (100 units) → tests far-field behavior";

    // Single massive Gaussian encompassing entire scene
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 50.0f),
        UnitQuaternion::identity(),
        make_float3(100.0f, 100.0f, 100.0f),  // extremely large
        make_float3(0.0f, 1.0f, 1.0f),  // cyan
        0.01f  // very low density so we can see through it
    ));

    // Smaller Gaussian inside for reference
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 3.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(1.0f, 0.0f, 0.0f),  // red
        0.5f
    ));

    return scene;
}

TestScene near_coincident_surfaces() {
    TestScene scene;
    scene.name = "near_coincident_surfaces";
    scene.description = "Two Gaussians separated by ~1e-5 → tests HIT_COINCIDENCE_EPS boundary";

    // Two Gaussians very close but not exactly coincident
    // Tests the HIT_COINCIDENCE_EPS threshold (1e-6)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(1.0f, 0.0f, 0.0f),  // red
        0.5f
    ));

    // Second Gaussian offset by 1e-5 units (just above epsilon threshold)
    scene.primitives.push_back(Primitive(
        make_float3(0.00001f, 0.0f, 0.0f),  // 1e-5 offset
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(0.0f, 0.0f, 1.0f),  // blue
        0.5f
    ));

    return scene;
}

// ─────────────────────────────────────────────────────────────────────
// Edge Case Tests: Priority 2 (Correctness)
// ─────────────────────────────────────────────────────────────────────

TestScene collinear_with_gaps() {
    TestScene scene;
    scene.name = "collinear_with_gaps";
    scene.description = "Non-overlapping Gaussians along Z-axis → tests sequential entry/exit handling";

    // Five Gaussians in a line with clear gaps between them
    // Scale 0.4 with spacing 1.5 ensures no overlap
    const float spacing = 1.5f;
    const auto scale = make_float3(0.4f, 0.4f, 0.4f);

    for (int i = 0; i < 5; ++i) {
        // Rainbow colors
        float r = (i == 0 || i == 4) ? 1.0f : 0.0f;
        float g = (i == 1 || i == 2) ? 1.0f : 0.0f;
        float b = (i == 2 || i == 3 || i == 4) ? 1.0f : 0.0f;

        scene.primitives.push_back(Primitive(
            make_float3(0.0f, 0.0f, static_cast<float>(i) * spacing + 1.0f),
            UnitQuaternion::identity(),
            scale,
            make_float3(r, g, b),
            0.5f
        ));
    }

    return scene;
}

TestScene nested_off_center() {
    TestScene scene;
    scene.name = "nested_off_center";
    scene.description = "Small Gaussian near edge of large one → tests eccentric nesting";

    // Large outer Gaussian
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.5f, 1.5f, 1.5f),
        make_float3(1.0f, 0.0f, 0.0f),  // red
        0.2f
    ));

    // Small Gaussian near the edge (not centered)
    // At position (1.0, 0, 0), it's near the boundary of the outer Gaussian
    scene.primitives.push_back(Primitive(
        make_float3(1.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.3f, 0.3f, 0.3f),
        make_float3(0.0f, 0.0f, 1.0f),  // blue
        0.8f
    ));

    return scene;
}

TestScene chain_overlaps() {
    TestScene scene;
    scene.name = "chain_overlaps";
    scene.description = "A overlaps B, B overlaps C, but A and C don't overlap → tests local overlap handling [COLORS SWAPPED FOR DEBUG]";

    // Gaussian A (left) - BLUE now
    scene.primitives.push_back(Primitive(
        make_float3(-0.8f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(0.0f, 0.0f, 1.0f),  // blue (SWAPPED)
        0.5f
    ));

    // Gaussian B (center) - overlaps both A and C - GREEN unchanged
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(0.0f, 1.0f, 0.0f),  // green
        0.5f
    ));

    // Gaussian C (right) - RED now
    scene.primitives.push_back(Primitive(
        make_float3(0.8f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(1.0f, 0.0f, 0.0f),  // red (SWAPPED)
        0.5f
    ));

    return scene;
}

// ─────────────────────────────────────────────────────────────────────
// Edge Case Tests: Priority 3 (Completeness)
// ─────────────────────────────────────────────────────────────────────

TestScene rotation_180_degrees() {
    TestScene scene;
    scene.name = "rotation_180_degrees";
    scene.description = "180-degree rotation → tests quaternion edge case (w≈0) [COLORS SWAPPED FOR DEBUG]";

    const auto scale = make_float3(1.0f, 0.3f, 0.3f);  // elongated

    // No rotation (reference) - BLUE now
    scene.primitives.push_back(Primitive(
        make_float3(-1.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        scale,
        make_float3(0.0f, 0.0f, 1.0f),  // blue (SWAPPED)
        0.5f
    ));

    // 180-degree rotation around Z axis - RED now
    // Quaternion: (w, x, y, z) = (0, 0, 0, 1) for 180° around Z
    scene.primitives.push_back(Primitive(
        make_float3(1.0f, 0.0f, 0.0f),
        UnitQuaternion::from_unchecked(0.0f, 0.0f, 0.0f, 1.0f),
        scale,
        make_float3(1.0f, 0.0f, 0.0f),  // red (SWAPPED)
        0.5f
    ));

    return scene;
}

TestScene high_optical_thickness() {
    TestScene scene;
    scene.name = "high_optical_thickness";
    scene.description = "Very high sigma_t (10.0) → tests MAX_OPTICAL_DEPTH clamping";

    // Dense fog with very high extinction coefficient
    // scale=0.5 keeps bounding sphere radius=0.5, camera at z=-1 is clearly outside
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.5f, 0.5f, 0.5f),
        make_float3(0.8f, 0.8f, 0.8f),  // light gray
        10.0f  // very high density
    ));

    return scene;
}

TestScene low_optical_thickness() {
    TestScene scene;
    scene.name = "low_optical_thickness";
    scene.description = "Very low sigma_t (0.001) → tests nearly transparent medium";

    // Nearly transparent volume
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.0f, 1.0f, 1.0f),
        make_float3(0.0f, 1.0f, 1.0f),  // cyan
        0.001f  // very low density
    ));

    return scene;
}

TestScene zero_albedo() {
    TestScene scene;
    scene.name = "zero_albedo";
    scene.description = "Pure absorption (albedo = 0) → path terminates immediately on scattering";

    // Black absorbing medium (no scattering, only absorption)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(1.0f, 1.0f, 1.0f),
        make_float3(0.0f, 0.0f, 0.0f),  // black (pure absorption)
        0.5f
    ));

    return scene;
}

// ─────────────────────────────────────────────────────────────────────
// Debug Tests (Minimal Reproducible Failures)
// ─────────────────────────────────────────────────────────────────────

TestScene debug_single_at_origin() {
    TestScene scene;
    scene.name = "debug_single_at_origin";
    scene.description = "Single Gaussian at (0,0,0) - minimal test for hang";

    // Single Gaussian at origin (same position as center Gaussian in transform_translation)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),        // at origin
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),        // same scale as transform_translation
        make_float3(0.0f, 1.0f, 1.0f),        // cyan
        0.5f                                  // same sigma_t
    ));

    return scene;
}

TestScene debug_single_offset() {
    TestScene scene;
    scene.name = "debug_single_offset";
    scene.description = "Single Gaussian at (1,0,0) - test if offset prevents hang";

    // Single Gaussian offset from origin
    scene.primitives.push_back(Primitive(
        make_float3(1.0f, 0.0f, 0.0f),        // offset right
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(0.0f, 1.0f, 1.0f),        // cyan
        0.5f
    ));

    return scene;
}

TestScene debug_two_at_origin() {
    TestScene scene;
    scene.name = "debug_two_at_origin";
    scene.description = "Two Gaussians at (0,0,0) - test if coincident surfaces cause hang";

    // Two coincident Gaussians (like coincident_surfaces test, but with transform_translation params)
    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(1.0f, 0.0f, 0.0f),        // red
        0.5f
    ));

    scene.primitives.push_back(Primitive(
        make_float3(0.0f, 0.0f, 0.0f),
        UnitQuaternion::identity(),
        make_float3(0.4f, 0.4f, 0.4f),
        make_float3(0.0f, 0.0f, 1.0f),        // blue
        0.5f
    ));

    return scene;
}

TestScene debug_grid_2x2() {
    TestScene scene;
    scene.name = "debug_grid_2x2";
    scene.description = "2×2 grid - test if grid size matters";

    const auto rotation = UnitQuaternion::identity();
    const auto scale = make_float3(0.4f, 0.4f, 0.4f);
    const auto albedo = make_float3(0.0f, 1.0f, 1.0f);  // cyan
    constexpr float sigma_t = 0.5f;

    // 2x2 grid (4 Gaussians)
    for (int y = 0; y <= 1; ++y) {
        for (int x = 0; x <= 1; ++x) {
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
// Get Test Scenes by Category
// ─────────────────────────────────────────────────────────────────────

std::vector<TestScene> get_validation_test_scenes() {
    return {
        // ===== Core Correctness Tests =====
        single_purple(),
        coincident_surfaces(),
        partial_overlap(),
        total_overlap(),
        depth_ordering(),
        camera_inside(),
        non_overlapping(),

        // ===== Transform Tests =====
        transform_scale(),
        transform_rotation(),
        transform_translation(),

        // ===== Stress Tests =====
        many_gaussians(),
        nested_structure(),
        tangent_rays(),

        // ===== Edge Case Tests: Priority 0 (Critical) =====
        empty_scene(),
        hit_buffer_at_capacity(),
        hit_buffer_overflow(),
        ray_at_exact_boundary(),
        all_behind_camera(),

        // ===== Edge Case Tests: Priority 1 (Numerical Stability) =====
        extreme_anisotropic_scale(),
        extreme_small_scale(),
        extreme_large_scale(),
        near_coincident_surfaces(),

        // ===== Edge Case Tests: Priority 2 (Correctness) =====
        collinear_with_gaps(),
        nested_off_center(),
        chain_overlaps(),

        // ===== Edge Case Tests: Priority 3 (Completeness) =====
        rotation_180_degrees(),
        high_optical_thickness(),
        low_optical_thickness(),
        zero_albedo(),

        // ===== Debug Tests (Minimal Cases) =====
        debug_single_at_origin(),
        debug_single_offset(),
        debug_two_at_origin(),
        debug_grid_2x2(),
        minimal_behind_camera(),
        minimal_in_front(),
        multiple_same_z(),
    };
}

std::vector<TestScene> get_stress_test_scenes() {
    return {
        stress_256_gaussians(),
        stress_512_gaussians(),
        stress_1024_gaussians(),
        stress_2048_gaussians(),
        stress_4096_gaussians(),
        stress_8192_gaussians(),
    };
}

std::vector<TestScene> get_all_test_scenes() {
    auto validation = get_validation_test_scenes();
    auto stress = get_stress_test_scenes();

    // Combine both vectors
    validation.insert(validation.end(), stress.begin(), stress.end());
    return validation;
}

}  // namespace thesis::test::scenes
