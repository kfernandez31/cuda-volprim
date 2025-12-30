#pragma once

#include "thesis/host/params/primitive.h"

#include <string>
#include <vector>

namespace thesis::test::scenes {

// Test scene descriptor
struct TestScene {
    std::string name;
    std::string description;
    std::vector<thesis::host::params::Primitive> primitives;
};

// Core correctness tests
TestScene coincident_surfaces();
TestScene partial_overlap();
TestScene total_overlap();
TestScene depth_ordering();
TestScene camera_inside();
TestScene non_overlapping();

// Transform tests
TestScene transform_scale();
TestScene transform_rotation();
TestScene transform_translation();

// Stress tests
TestScene many_gaussians();
TestScene nested_structure();
TestScene tangent_rays();

// Edge case tests: Priority 0 (Critical - Could Crash/Hang)
TestScene empty_scene();
TestScene hit_buffer_at_capacity();
TestScene hit_buffer_overflow();
TestScene ray_at_exact_boundary();
TestScene all_behind_camera();

// Edge case tests: Priority 1 (Numerical Stability)
TestScene extreme_anisotropic_scale();
TestScene extreme_small_scale();
TestScene extreme_large_scale();
TestScene near_coincident_surfaces();

// Edge case tests: Priority 2 (Correctness)
TestScene collinear_with_gaps();
TestScene nested_off_center();
TestScene chain_overlaps();

// Edge case tests: Priority 3 (Completeness)
TestScene rotation_180_degrees();
TestScene high_optical_thickness();
TestScene low_optical_thickness();
TestScene zero_albedo();

// Debug tests (minimal reproducible failures)
TestScene debug_single_at_origin();
TestScene debug_single_offset();
TestScene debug_two_at_origin();
TestScene debug_grid_2x2();
TestScene minimal_behind_camera();
TestScene minimal_in_front();
TestScene multiple_same_z();

// Get all test scenes
std::vector<TestScene> get_all_test_scenes();

}  // namespace thesis::test::scenes
