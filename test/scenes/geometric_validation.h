#pragma once

#include "thesis/device/params/primitive.h"

#include <string>
#include <vector>

namespace thesis::test::scenes {

// Test scene descriptor
struct TestScene {
    std::string name;
    std::string description;
    std::vector<thesis::device::params::Primitive> primitives;
};

// Core correctness tests
TestScene single_purple();
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

// Performance stress tests (increasing gaussian counts)
TestScene stress_16_gaussians();
TestScene stress_256_gaussians();
TestScene stress_512_gaussians();
TestScene stress_1024_gaussians();
TestScene stress_2048_gaussians();
TestScene stress_4096_gaussians();
TestScene stress_8192_gaussians();

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

// Get test scenes by category
std::vector<TestScene> get_all_test_scenes();
std::vector<TestScene> get_validation_test_scenes();
std::vector<TestScene> get_stress_test_scenes();

}  // namespace thesis::test::scenes
