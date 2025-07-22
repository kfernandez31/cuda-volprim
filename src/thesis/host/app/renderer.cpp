#include "thesis/host/app/renderer.h"

#include "thesis/pch.h"

#include "thesis/common/utils/types.h"
#include "thesis/device/utils/vector.h"
#include "thesis/host/geometry/mesh.h"
#include "thesis/host/optix/logging.h"
#include "thesis/host/params/primitive.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/data.h"
#include "thesis/host/utils/result.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <sutil/vec_math.h>
#include <utility>
#include <vector>

#define NUM_PRIMITIVES 3

namespace thesis::host::app {

using Ico = geometry::Icosphere<ICOSPHERE_N>;

Renderer::Renderer(const app::Config& config)
    // clang-format off
    : config_(config),
      cuda_ctx_(),
      optix_ctx_(cuda_ctx_.get()),
      streams_(),
      gas_(1, cuda_ctx_.get(), streams_[cuda::StreamKind::GAS]),
      ias_(cuda_ctx_.get(), streams_[cuda::StreamKind::IAS]),
      instances_(NUM_PRIMITIVES, cuda_ctx_.get(), streams_[cuda::StreamKind::GAS], cuda::AllocType::OnBoth),
      sbt_(cuda_ctx_.get(), streams_[cuda::StreamKind::SBT], NUM_PRIMITIVES),
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

void Renderer::initPrimsAndGAS()
{
    /* ── 1. Per-primitive data ────────────────────────────────────── */
    const glm::vec3 albedo [NUM_PRIMITIVES] = {
        glm::vec3{1.f,0.f,0.f},
        glm::vec3{0.f,1.f,0.f},
        glm::vec3{0.f,0.f,1.f}
    };
    const glm::vec3 translate[NUM_PRIMITIVES] = {
        glm::vec3{-1.5f,0.f,0.5f},
        glm::vec3{ 0.0f,0.f,0.5f},
        glm::vec3{+1.5f,0.f,0.5f}
    };
    const glm::vec3 scale[NUM_PRIMITIVES] = {
        glm::vec3{0.3f}, glm::vec3{0.3f}, glm::vec3{0.3f}
    };

    spdlog::info("hello 0");

    /* ── 2. Build BLAS with one unit sphere ───────────────────────── */
    gas_.host()[0] = make_float4(0.f, 0.f, 0.f, 1.f);   // centre, radius
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);

    spdlog::info("hello 0");

    /* ── 3. Fill primitives_[] and OptixInstance[] ────────────────── */
    for (uint32_t i = 0; i < NUM_PRIMITIVES; ++i)
    {
        params::Primitive prim(
            translate[i],
            glm::quat(1, 0, 0, 0),
            scale[i],
            albedo[i],
            1.f);
        primitives_[i] = prim.toDevice();

        OptixInstance inst{};
        const auto Mt = glm::transpose(prim.localToWorld()); // row-major
        std::memcpy(inst.transform, glm::value_ptr(Mt), 12 * sizeof(float));

        inst.traversableHandle = gas_.get();
        inst.instanceId        = i;
        inst.sbtOffset         = i;
        inst.visibilityMask    = 0xFF;
        inst.flags             = OPTIX_INSTANCE_FLAG_NONE;
        instances_[i]          = inst;
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);

    instances_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::IAS);

    /* ── 4. Build IAS over instances ──────────────────────────────── */
    OptixBuildInput bi{};
    bi.type                       = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    bi.instanceArray.instances    = instances_.cu_device_ptr();
    bi.instanceArray.numInstances = NUM_PRIMITIVES;

    ias_.build(bi, cuda_ctx_.get(), optix_ctx_.get());
}

void Renderer::uploadParams() {
    auto& par = launch_params_[0];
    par.seed_ = config_.seed_;
    par.debug_ = config_.debug_;
    par.gas_handle_ = ias_.get();
    par.camera_ = camera_.toDevice();
    par.env_map_ = env_map_.toDevice();
    par.image_ = image_.toDevice();
    par.primitives_ = device::utils::DynamicVector<device::params::Primitive>(primitives_.device(), primitives_.size());

    launch_params_.upload();
}

void Renderer::createPipeline() {
    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = config_.launch_params_variable_name_.c_str();
    pco.numPayloadValues = device::payloads::MAX_PAYLOADS_IN_USE;
    pco.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;

    // module
    module_ = utils::try_unwrap_or_exit<optix::Module>(
        optix::Module::load(optix_ctx_.get(), config_.module_blob_path_, pco)
    );

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
        config_.anyhit_function_name_.empty() ? nullptr : config_.anyhit_function_name_.c_str()
    );

    // sbt
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

    spdlog::info("Launching OptiX pipeline - will render {} Gaussians...", NUM_PRIMITIVES);
    pipeline_.launch(streams_[cuda::StreamKind::Main]->get(), launch_params_.cu_device_ptr(),
                     sizeof(common::params::LaunchParams), sbt_.get(),
                     image_.width(),
                     image_.height(),
                     image_.num_samples_per_pixel());
    spdlog::info("Pipeline execution complete");

    utils::try_unwrap_or_exit(image_.save(config_.output_path_));
}

}  // namespace thesis::host::app
