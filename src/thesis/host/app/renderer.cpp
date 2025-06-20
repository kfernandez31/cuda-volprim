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

#define ICOSPHERE_N 0
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
    
    initGAS();
    initPrimitives();
    createPipeline();
}

void Renderer::initGAS() {
    using geometry::Icosphere;

    std::vector<Icosphere<ICOSPHERE_N>> icos;
    // icos.emplace_back(glm::translate(glm::vec3(-2.0f, 0.0f, 0.5f)));
    icos.emplace_back(glm::translate(glm::vec3(0.0f, 0.0f, 0.5f)));
    // icos.emplace_back(glm::translate(glm::vec3(2.0f, 0.0f, 0.5f)));

    for (size_t i = 0; i < icos.size(); ++i) {
        gas_.upload_batch_from(i, icos[i]);
    }

    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);
}

void Renderer::initPrimitives() {
    std::vector<host::params::Primitive> host_primitives;

    glm::vec3 colors[NUM_PRIMITIVES] = {
        // glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        // glm::vec3(0.0f, 0.0f, 1.0f),
    };

    glm::vec3 translations[NUM_PRIMITIVES] = {
        // glm::vec3(-2.0f, 0.0f, 0.0f),
        glm::vec3(0.0f,  0.0f, 0.5f),
        // glm::vec3(+2.0f, 0.0f, 0.5f),
    };

    for (size_t i = 0; i < NUM_PRIMITIVES; ++i) {
        // auto color = glm::vec3(static_cast<float>(i) / static_cast<float>(NUM_PRIMITIVES));
        auto albedo = colors[i];
        auto translation = glm::translate(translations[i]);
        auto optical_depth_scale = 500.0f;

        // clang-format off
        host_primitives.emplace_back(
            translation,
            glm::identity<glm::mat4>(),
            glm::scale(glm::vec3(0.3f)),
            albedo, 
            optical_depth_scale
        );
    }

    std::transform(host_primitives.begin(), host_primitives.end(), primitives_.host(), [](const auto& p) { return p.toDevice(); });
    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);
}

void Renderer::uploadParams() {
    auto& par = launch_params_[0];
    par.seed_ = config_.seed_;
    par.num_triangles_per_primitive_ = geometry::Icosphere<ICOSPHERE_N>::NumIndices;
    par.gas_handle_ = gas_.get();
    par.camera_ = camera_.toDevice();
    par.env_map_ = env_map_.toDevice();
    par.image_ = image_.toDevice();
    par.primitives_ = device::utils::DynamicVector<device::params::Primitive>(primitives_.device(), primitives_.size());

    launch_params_.upload();
}

void Renderer::createRaygenPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    desc.raygen.module = module_.get();
    desc.raygen.entryFunctionName = config_.raygen_function_name_.c_str();

    raygen_pg_ = optix::ProgramGroup(optix_ctx_.get(), desc);
    spdlog::debug("Raygen program group created");
}

void Renderer::createMissPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    desc.miss.module = module_.get();
    desc.miss.entryFunctionName = config_.miss_function_name_.c_str();

    miss_pg_ = optix::ProgramGroup(optix_ctx_.get(), desc);
    spdlog::debug("Miss program group created");
}

void Renderer::createHitgroupPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;

    desc.hitgroup.moduleCH = module_.get();
    desc.hitgroup.entryFunctionNameCH = config_.closesthit_function_name_.c_str();

    desc.hitgroup.moduleAH = module_.get();
    desc.hitgroup.entryFunctionNameAH = config_.anyhit_function_name.c_str();

    hitgroup_pg_ = optix::ProgramGroup(optix_ctx_.get(), desc);
    spdlog::debug("Hitgroup program group created");
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

    createRaygenPG();
    createMissPG();
    createHitgroupPG();

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
