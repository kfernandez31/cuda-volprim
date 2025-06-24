#include "thesis/host/app/renderer.h"

#include "thesis/pch.h"

#include "thesis/common/utils/types.h"
#include "thesis/device/payloads/registry.h"
#include "thesis/device/utils/vector.h"
#include "thesis/host/geometry/mesh.h"
#include "thesis/host/optix/logging.h"
#include "thesis/host/optix/ptx.h"
#include "thesis/host/params/primitive.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/data.h"
#include "thesis/host/utils/result.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <sutil/vec_math.h>
#include <utility>
#include <vector>

#define ICOSPHERE_N 1
#define NUM_PRIMITIVES 1
#define NUM_TOTAL_INDICES (NUM_PRIMITIVES * geometry::Icosphere<ICOSPHERE_N>::NumIndices)
#define NUM_TOTAL_VERTICES (NUM_PRIMITIVES * geometry::Icosphere<ICOSPHERE_N>::NumVertices)

namespace thesis::host::app {

Renderer::Renderer(const app::Config& config)
    // clang-format off
    : config_(config),
      cuda_ctx_(),
      optix_ctx_(cuda_ctx_.get()),
      streams_(),
      gas_(NUM_TOTAL_VERTICES, NUM_TOTAL_INDICES, cuda_ctx_.get(), streams_[cuda::StreamKind::GAS]),
      sbt_(cuda_ctx_.get(), streams_[cuda::StreamKind::SBT]),
      env_map_(config_.env_map_path_, cuda_ctx_.get(), streams_[cuda::StreamKind::EnvMap]),
      image_(config_.image_width_, config_.image_height_, config_.num_samples_per_pixel_, cuda_ctx_.get(), streams_[cuda::StreamKind::Image], streams_[cuda::StreamKind::Main]),
      camera_(host::params::Camera::getDefaultCamera(config.image_width_, config.image_height_)),
      primitives_(NUM_PRIMITIVES, cuda_ctx_.get(), streams_[cuda::StreamKind::Prims], cuda::AllocType::OnBoth),
      launch_params_(1, cuda_ctx_.get(), streams_[cuda::StreamKind::Main], cuda::AllocType::OnBoth) {
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::EnvMap);
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Image);
    
    initPrimsAndGAS();
    createPipeline();
}

// 1. Small Gaussian inside Big Gaussian
/*
void Renderer::initPrimsAndGAS() {
    glm::vec3 albedos[NUM_PRIMITIVES] = {
        glm::vec3(1.0f, 1.0f, 0.0f), // outer yellow
        glm::vec3(1.0f, 0.0f, 0.0f), // inner red
    };

    glm::vec3 translations[NUM_PRIMITIVES] = {
        glm::vec3(0.0f, 0.0f, 0.5f), // both at same position
        glm::vec3(0.0f, 0.0f, 0.5f),
    };

    glm::mat4 rotations[NUM_PRIMITIVES] = {
        glm::identity<glm::mat4>(),
        glm::identity<glm::mat4>(),
    };

    glm::vec3 scales[NUM_PRIMITIVES] = {
        glm::vec3(1.0f), // large
        glm::vec3(0.1f), // small
    };

    float od_scales[NUM_PRIMITIVES] = {
        100.0f, // transparent-ish
        1000.0f, // dense
    };

    for (int i = 0; i < NUM_PRIMITIVES; ++i) {
        host::params::Primitive prim(
            glm::translate(translations[i]),
            rotations[i],
            glm::scale(scales[i]),
            albedos[i],
            od_scales[i]
        );
        primitives_.host()[i] = prim.toDevice();
        geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
        gas_.upload_batch_from(i, ico);
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}
*/

// 2. Lined-Up Gaussians (side by side)
/*
void Renderer::initPrimsAndGAS() {
    for (int i = 0; i < NUM_PRIMITIVES; ++i) {
        float x = -2.0f + i * 1.0f;
        glm::vec3 color = glm::vec3(
            static_cast<float>(i) / NUM_PRIMITIVES,
            1.0f - static_cast<float>(i) / NUM_PRIMITIVES,
            0.5f
        );

        host::params::Primitive prim(
            glm::translate(glm::vec3(x, 0.0f, 0.5f)),
            glm::identity<glm::mat4>(),
            glm::scale(glm::vec3(0.4f)),
            color,
            300.0f
        );
        primitives_.host()[i] = prim.toDevice();
        geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
        gas_.upload_batch_from(i, ico);
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}
*/

// 2.5. Lined-Up Gaussians (one behind other)
/*
void Renderer::initPrimsAndGAS() {
    const glm::vec3 colors[NUM_PRIMITIVES] = {
        glm::vec3(1.0f, 0.0f, 0.0f), // red
        glm::vec3(0.0f, 1.0f, 0.0f), // green
        glm::vec3(0.0f, 0.0f, 1.0f), // blue
    };

    for (int i = 0; i < NUM_PRIMITIVES; ++i) {
        float z = 0.3f + i * 0.3f; // further into screen
        glm::vec3 position(0.0f, 0.0f, z);

        host::params::Primitive prim(
            glm::translate(position),
            glm::identity<glm::mat4>(),
            glm::scale(glm::vec3(0.4f)),
            colors[i],
            500.0f
        );

        primitives_.host()[i] = prim.toDevice();
        geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
        gas_.upload_batch_from(i, ico);
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}
*/

// 3. Overlapping Gaussians
/*
void Renderer::initPrimsAndGAS() {
    const glm::vec3 colors[NUM_PRIMITIVES] = {
        glm::vec3(1.0f, 0.0f, 0.0f), // red
        glm::vec3(0.0f, 1.0f, 0.0f), // green
        glm::vec3(0.0f, 0.0f, 1.0f), // blue
    };

    for (int i = 0; i < NUM_PRIMITIVES; ++i) {
        float x = -0.3f + i * 0.3f;
        glm::vec3 position(x, 0.0f, 0.5f); // horizontally aligned, intersecting

        host::params::Primitive prim(
            glm::translate(position),
            glm::identity<glm::mat4>(),
            glm::scale(glm::vec3(0.5f)),
            colors[i],
            600.0f
        );

        primitives_.host()[i] = prim.toDevice();
        geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
        gas_.upload_batch_from(i, ico);
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}
*/

// 4. Anisotropic Gaussian
/*
void Renderer::initPrimsAndGAS() {
    host::params::Primitive prim(
        glm::translate(glm::vec3(0.0f, 0.0f, 0.5f)),
        glm::identity<glm::mat4>(),
        glm::scale(glm::vec3(0.2f, 1.0f, 0.2f)),  // Tall on Y
        glm::vec3(0.7f, 0.3f, 0.9f),
        600.0f
    );

    primitives_.host()[0] = prim.toDevice();
    geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
    gas_.upload_batch_from(0, ico);

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}
*/

// 5. Rotated Gaussian // TODO(kacper): fix!!!!

void Renderer::initPrimsAndGAS() {

    const auto R = glm::rotate(glm::radians(config_.angle_), glm::vec3(0, 1, 0));

    host::params::Primitive prim(
        glm::translate(glm::vec3(0.0f, 0.0f, 0.5f)),
        R,
        glm::scale(glm::vec3(1.0f, 0.2f, 0.2f)),
        glm::vec3(1.0f, 0.0f, 0.0f),
        600.0f
    );

    primitives_.host()[0] = prim.toDevice();
    geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
    gas_.upload_batch_from(0, ico);

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}

//  6. RGB Gradient Stack (Z-layered)
/*
void Renderer::initPrimsAndGAS() {
    glm::vec3 colors[3] = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
    };

    for (int i = 0; i < NUM_PRIMITIVES  ; ++i) {
        float z = 0.3f + i * 0.3f;

        host::params::Primitive prim(
            glm::translate(glm::vec3(0.0f, 0.0f, z)),
            glm::identity<glm::mat4>(),
            glm::scale(glm::vec3(0.4f)),
            colors[i],
            300.0f
        );

        primitives_.host()[i] = prim.toDevice();
        geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
        gas_.upload_batch_from(i, ico);
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}
*/

// 7. Fade-Out Line (Opacity Gradient)
/*
void Renderer::initPrimsAndGAS() {
    for (int i = 0; i < NUM_PRIMITIVES; ++i) {
        float x = -2.0f + i * 1.0f;
        float intensity = 1.0f - i * 0.2f; // fades from left to right
        float od = 100.0f + i * 100.0f;

        host::params::Primitive prim(
            glm::translate(glm::vec3(x, 0.0f, 0.5f)),
            glm::identity<glm::mat4>(),
            glm::scale(glm::vec3(0.4f)),
            glm::vec3(intensity, intensity, intensity),
            od
        );

        primitives_.host()[i] = prim.toDevice();
        geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
        gas_.upload_batch_from(i, ico);
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}
*/

// 8. RGB side by side
/*
void Renderer::initPrimsAndGAS() {
    glm::vec3 albedos[NUM_PRIMITIVES] = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
    };

    glm::vec3 translations[NUM_PRIMITIVES] = {
        glm::vec3(-1.5f, 0.0f, 0.5f),
        glm::vec3(0.0f, 0.0f, 0.5f),
        glm::vec3(+1.5f, 0.0f, 0.5f),
    };

    glm::mat4 rotations[NUM_PRIMITIVES] = {
        glm::identity<glm::mat4>(),
        glm::identity<glm::mat4>(),
        glm::identity<glm::mat4>(),
    };

    for (size_t i = 0; i < NUM_PRIMITIVES; ++i) {
        host::params::Primitive prim(
            glm::translate(translations[i]),
            rotations[i],
            glm::scale(glm::vec3(0.3f)),
            albedos[i], // glm::vec3(static_cast<float>(i) / static_cast<float>(NUM_PRIMITIVES));
            500.0f
        );

        // create prim
        primitives_.host()[i] = prim.toDevice();

        // create prim's hitbox
        geometry::Icosphere<ICOSPHERE_N> ico(prim.M());
        gas_.upload_batch_from(i, ico);
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);

    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}
*/

void Renderer::uploadParams() {
    auto& par = launch_params_[0];
    par.seed_ = config_.seed_;
    par.debug_ = config_.debug_;
    par.num_triangles_per_primitive_ = geometry::Icosphere<ICOSPHERE_N>::NumIndices;
    par.gas_handle_ = gas_.get();
    par.camera_ = camera_.toDevice();
    par.env_map_ = env_map_.toDevice();
    par.image_ = image_.toDevice();
    par.primitives_ = device::utils::DynamicVector<device::params::Primitive>(primitives_.device(), primitives_.size());

    launch_params_.upload();
}

void Renderer::createPipeline() {
    auto ptx =
        utils::try_unwrap_or_exit<optix::PTX>(optix::PTX::load(config_.ptx_path_));
    spdlog::info("PTX loaded ({} bytes)", ptx.size()); // TODO(kacper): load async

    OptixModuleCompileOptions mco = {};

    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = config_.launch_params_variable_name_.c_str();
    pco.numPayloadValues = device::payloads::MAX_PAYLOADS_IN_USE;

    module_ = optix::Module(optix_ctx_.get(), mco, pco, ptx.data());

    // raygen
    raygen_pg_ = optix::ProgramGroup::createRaygen(
        optix_ctx_.get(),
        module_.get(),
        config_.raygen_function_name_.c_str()
    );

    // miss
    miss_pg_ = optix::ProgramGroup::createMiss(
        optix_ctx_.get(),
        module_.get(),
        config_.miss_function_name_.c_str()
    );

    // hitgroup
    hitgroup_pg_ = optix::ProgramGroup::createHitgroup(
        optix_ctx_.get(),
        module_.get(),
        config_.closesthit_function_name_.c_str(),
        config_.anyhit_function_name.empty() ? nullptr : config_.anyhit_function_name.c_str()
    );

    sbt_.build(raygen_pg_.get(), miss_pg_.get(), hitgroup_pg_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::SBT);
    spdlog::info("SBT created");

    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;

    std::array<OptixProgramGroup, 3> pgs = {raygen_pg_.get(), miss_pg_.get(), hitgroup_pg_.get()};

    pipeline_ = optix::Pipeline(optix_ctx_.get(), pco, plo, pgs.data(), pgs.size());
    spdlog::info("OptiX pipeline built");
}

void Renderer::render() {
    uploadParams();

    spdlog::info("Launching OptiX pipeline...");
    pipeline_.launch(streams_[cuda::StreamKind::Main]->get(), launch_params_.cu_device_ptr(),
                     sizeof(common::params::LaunchParams), sbt_.get(),
                     image_.width(),
                     image_.height(),
                     image_.num_samples_per_pixel());
    spdlog::info("Pipeline execution complete");

    utils::try_unwrap_or_exit(image_.save(config_.output_path_));
}

}  // namespace thesis::host::app
