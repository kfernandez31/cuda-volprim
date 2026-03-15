#include "thesis/pch.h"

#include "thesis/common/geometry/quat.h"
#include "thesis/host/app/config.h"
#include "thesis/host/app/logging.h"
#include "thesis/host/app/renderer.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/utils/result.h"

#ifndef OPTIX_FUNCTION_TABLE_INCLUDED
#define OPTIX_FUNCTION_TABLE_INCLUDED
#include <optix_function_table_definition.h>
#endif  // OPTIX_FUNCTION_TABLE_INCLUDED

#include <utility>
#include <vector>

using namespace thesis::host;
using Primitive = thesis::device::params::Primitive;

static std::vector<Primitive> createDefaultScene() {
    // Default scene: single purple Gaussian at origin
    const auto translation = make_float3(0.0f, 0.0f, 0.0f);
    const auto rotation = thesis::common::geometry::UnitQuaternion::identity();
    const auto scale = make_float3(0.5f, 0.5f, 0.5f);
    const auto albedo = make_float3(0.5f, 0.0f, 0.5f);
    constexpr float sigma_t = 0.5f;

    return {Primitive::from_forward_quat(translation, rotation, scale, albedo, sigma_t)};
}

int main(int argc, char* argv[]) {
    app::logging::init();

    auto config = utils::try_unwrap_or_exit(app::Config::parse(argc, argv));
    app::Renderer renderer(config, createDefaultScene());
    renderer.render();

    return 0;
}