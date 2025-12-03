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

// Correctness tests
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

// Get all test scenes
std::vector<TestScene> get_all_test_scenes();

}  // namespace thesis::test::scenes
