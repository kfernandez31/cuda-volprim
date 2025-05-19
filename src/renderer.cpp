#include "thesis/renderer.h"

#include "thesis/pch.h"

#include "thesis/geometry/mesh.h"
#include "thesis/host/primitive.h"
#include "thesis/optix/logging.h"
#include "thesis/optix/ptx_handle.h"
#include "thesis/utils/check.h"
#include "thesis/utils/data.h"
#include "thesis/utils/glm.h"


#include <optional>
#include <filesystem>
#include <utility>
#include <spdlog/spdlog.h>
#include <string>
#include <sutil/vec_math.h>

#define MOCK_PRIMS // TODO(kacper): remove
#ifdef MOCK_PRIMS

#include <array>
#include <glm/glm.hpp>
#include <vector>

#define ICOSPHERE_N 0
#define NUM_PRIMITIVES 1
#endif // MOCK_PRIMS

namespace thesis {

Renderer::Renderer(const AppConfig& config)
    : config_(std::move(config)),
      cuda_ctx_(),
      optix_ctx_([&] {
          OPTIX_CHECK(optixInit());
          spdlog::debug("OptiX initialized");

          OptixDeviceContextOptions opts = {};
          opts.logCallbackFunction = &optix::contextLogCb;
          opts.logCallbackLevel = static_cast<int>(optix::LogLevel::Warning);
#ifdef DEBUG
          opts.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
#endif  // DEBUG

          return optix::DeviceContextHandle(opts, cuda_ctx_.get());
      }()),
      stream_(),
      env_map_(config_.env_map_path_),
      image_(fromWandH(config_.image_width_, config_.aspect_ratio_)), // TODO(kacper): move needed here?
      camera_([&] {
          host::Camera cam;
          cam.aspect_ratio_ = config_.aspect_ratio_;
          cam.image_width_ = config_.image_width_;
          cam.vertical_fov_ = 90.0f;
          cam.lookfrom_ = glm::vec3(0.0f, 0.0f, -2.0f);
          cam.lookat_ = glm::vec3(0.0f, 0.0f, 0.0f);
          cam.vup_ = glm::vec3(0.0f, 1.0f, 0.0f);
          cam.build();
          return cam;
      }()) {
    initGAS();
    createPipeline();
}

void Renderer::initGAS() {
    std::array<geometry::Icosphere<ICOSPHERE_N>, NUM_PRIMITIVES> icos;
    icos[0].translate(glm::vec3(0.0f, 0.0f, 0.5f));

    // icos[0].translate(glm::vec3(2.0f, 0.0f, 0.5f));
    // icos[1].translate(glm::vec3(0.0f, 0.0f, 0.5f));
    // icos[2].translate(glm::vec3(-2.0f, 0.0f, 0.5f));

    // Combine vertices
    std::vector<glm::vec3> all_vertices;
    for (const auto& ico : icos) {
        const auto& vs = ico.getVertices();
        all_vertices.insert(all_vertices.end(), vs.begin(), vs.end());
    }

    // Combine indices
    std::vector<glm::uvec3> all_indices;
    for (size_t i = 0; i < icos.size(); ++i) {
        auto offset = static_cast<unsigned int>(
            i *
            geometry::Icosphere<ICOSPHERE_N>::NumVertices);  // important, since indexing is local

        const auto& is = icos[i].getIndices();
        for (const auto& tri : is) {
            all_indices.emplace_back(tri + offset);
        }
    }

    gas_ = optix::TriangleGAS(stream_.get(), optix_ctx_.get(),
                              data::reinterpretSpan<float3, glm::vec3>(all_vertices),
                              data::reinterpretSpan<uint3, glm::uvec3>(all_indices));
}

void Renderer::uploadParams() {
    optix::LaunchParams par = {};
    par.gas_handle_ = gas_.get();
    par.num_samples_per_pixel_ = config_.num_samples_per_pixel_;
    par.num_primitives_ = NUM_PRIMITIVES;
    par.num_triangles_per_primitive_ = geometry::Icosphere<ICOSPHERE_N>::NumIndices;
    par.image_ = image_.toDevice();
    par.env_map_ = env_map_.toDevice();
    par.camera_ = camera_.toDevice();

    spdlog::info("created host primitives");
    std::vector<host::Primitive> host_primitives;
    for (size_t i = 0; i < NUM_PRIMITIVES; ++i) {
        // auto color = make_float3(static_cast<float>(i) /
        // static_cast<float>(par.num_primitives_));
        glm::vec3 albedo(0);
        float optical_depth_scale = 0;
        auto id = glm::identity<glm::mat4>();
        host_primitives.emplace_back(id, id, id, albedo, optical_depth_scale);
    }

    spdlog::info("create device primitives");
    primitives_ = cuda::Buffer<device::Primitive>(NUM_PRIMITIVES);
    for (size_t i = 0; i < NUM_PRIMITIVES; ++i) {
        spdlog::info("i = {}", i);
        primitives_.host()[i] = std::move(host_primitives[i].toDevice());
    }
    par.primitives_ = primitives_.upload();
    spdlog::info("uploaded device params");

    launch_params_ = cuda::Buffer<decltype(par)>::onDeviceOnly(&par, 1);
    spdlog::info("Uploaded launch params");
}

void Renderer::createRaygenPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    desc.raygen.module = module_.get();
    desc.raygen.entryFunctionName = config_.raygen_function_name_.c_str();

    raygen_pg_ = optix::ProgramGroupHandle(optix_ctx_.get(), desc);
    spdlog::debug("Raygen program group created");
}

void Renderer::createMissPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    desc.miss.module = module_.get();
    desc.miss.entryFunctionName = config_.miss_function_name_.c_str();

    miss_pg_ = optix::ProgramGroupHandle(optix_ctx_.get(), desc);
    spdlog::debug("Miss program group created");
}

void Renderer::createHitgroupPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;

    desc.hitgroup.moduleCH = module_.get();
    desc.hitgroup.entryFunctionNameCH = config_.closesthit_function_name_.c_str();

    desc.hitgroup.moduleAH = module_.get();
    desc.hitgroup.entryFunctionNameAH = config_.anyhit_function_name.c_str();

    hitgroup_pg_ = optix::ProgramGroupHandle(optix_ctx_.get(), desc);
    spdlog::debug("Hitgroup program group created");
}

void Renderer::createPipeline() {
    auto ptx = try_unwrap_or_exit<PtxHandle>(PtxHandle::load(config_.ptx_path_));
    spdlog::info("PTX loaded ({} bytes)", ptx.size());

    OptixModuleCompileOptions mco = {};

    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = config_.launch_params_variable_name_.c_str();
    pco.numPayloadValues = 3;

    module_ = optix::ModuleHandle(optix_ctx_.get(), mco, pco, ptx.data());

    createRaygenPG();
    createMissPG();
    createHitgroupPG();

    sbt_ = optix::SBTHandle(raygen_pg_.get(), miss_pg_.get(), hitgroup_pg_.get());
    spdlog::debug("SBT created");

    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;

    std::array<OptixProgramGroup, 3> pgs = {raygen_pg_.get(), miss_pg_.get(), hitgroup_pg_.get()};

    pipeline_ = optix::PipelineHandle(optix_ctx_.get(), pco, plo, pgs.data(), pgs.size());
    spdlog::info("OptiX pipeline built");
}

void Renderer::render() {
    uploadParams();

    spdlog::info("Launching OptiX pipeline...");
    pipeline_.launch(stream_.get(), reinterpret_cast<CUdeviceptr>(launch_params_.device()),
                     sizeof(optix::LaunchParams), sbt_.get(),
                     static_cast<unsigned int>(image_.width()),
                     static_cast<unsigned int>(image_.height()),
                     params_.samples_per_pixel_
                    );
    cuda::StreamHandle::synchronizeDevice();
    spdlog::info("Pipeline execution complete");

    try_unwrap_or_exit(image_.save());
    spdlog::info("Image saved to '{}'", config_.output_path_.string());
}

}  // namespace thesis
