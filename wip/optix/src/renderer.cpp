#include "thesis/renderer.h"

#include "thesis/pch.h"

#include "thesis/optix/logging.h"
#include "thesis/utils/check.h"
#include "thesis/utils/io.h"

#include <optional>
#include <spdlog/spdlog.h>
#include <string>

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
      env_map_(config_.env_map_path_),
      image_(config_.image_width_, config_.aspect_ratio_),
      camera_([&] {
          host::Camera cam;
          cam.aspect_ratio_ = config_.aspect_ratio_;
          cam.image_width_ = config_.image_width_;
          cam.vertical_fov_ = 90.0f;
          cam.lookfrom_ = glm::vec3(0.0f, 0.0f, 0.0f);
          cam.lookat_ = glm::vec3(0.0f, 0.0f, -1.0f);
          cam.vup_ = glm::vec3(0.0f, 1.0f, 0.0f);
          cam.build();
          return cam;
      }()),
      stream_() {
    initGAS();
    createPipeline();
}

void Renderer::initGAS() {
    // TODO(kacper): add more triangles
    std::vector<float3> vertices = {
        // Triangle 1
        float3{-0.5f, -0.5f, -1.0f},
        float3{ 0.5f, -0.5f, -1.0f},
        float3{ 0.0f,  0.5f, -1.0f},
        // Triangle 2
        float3{-0.5f,  0.5f, -1.0f},
        float3{ 0.5f,  0.5f, -1.0f},
        float3{ 0.0f, -0.5f, -1.0f},
    };

    std::vector<uint3> indices = {
        uint3{0, 1, 2}, // Triangle 1
        uint3{3, 4, 5}, // Triangle 2
    };

    gas_ = optix::TriangleGAS(stream_.get(), optix_ctx_.get(), vertices, indices);
    cuda::StreamHandle::synchronizeDevice();
}

void Renderer::createRaygenPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    desc.raygen.module = module_.get();
    desc.raygen.entryFunctionName = config_.raygen_function_name_.data();

    raygen_pg_ = optix::ProgramGroupHandle(optix_ctx_.get(), desc);
    spdlog::debug("Raygen program group created");
}

void Renderer::createMissPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    desc.miss.module = module_.get();
    desc.miss.entryFunctionName = config_.miss_function_name_.data();

    miss_pg_ = optix::ProgramGroupHandle(optix_ctx_.get(), desc);
    spdlog::debug("Miss program group created");
}

void Renderer::createHitgroupPG() {
    OptixProgramGroupDesc desc = {};
    desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    desc.hitgroup.moduleCH = module_.get();
    desc.hitgroup.entryFunctionNameCH = config_.hitgroup_function_name_.data();

    hitgroup_pg_ = optix::ProgramGroupHandle(optix_ctx_.get(), desc);
    spdlog::debug("Hitgroup program group created");
}

void Renderer::createPipeline() {
    auto ptx = try_unwrap_or_exit<std::string>(io::readFileToString(config_.ptx_path_));
    spdlog::info("PTX loaded ({} bytes)", ptx.size());

    OptixModuleCompileOptions mco = {};

    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = config_.launch_params_variable_name_.data();
    pco.numPayloadValues = 3;

    module_ = optix::ModuleHandle(optix_ctx_.get(), mco, pco, ptx);

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

void Renderer::uploadParams() {
    optix::LaunchParams par = {};
    par.gas_handle_ = gas_.get();
    par.num_samples_per_pixel_ = config_.num_samples_per_pixel_;
    par.image_ = image_.toDevice();
    par.env_map_ = env_map_.toDevice();
    par.camera_ = camera_.toDevice();

    launch_params_ = cuda::Buffer<decltype(par)>::onDeviceOnly(&par, 1);
    spdlog::info("Uploaded launch params");
}

void Renderer::render() {
    uploadParams();

    spdlog::info("Launching OptiX pipeline...");
    pipeline_.launch(stream_.get(), reinterpret_cast<CUdeviceptr>(launch_params_.device()),
                     sizeof(optix::LaunchParams), sbt_.get(),
                     static_cast<unsigned int>(image_.width()),
                     static_cast<unsigned int>(image_.height()));
    cuda::StreamHandle::synchronizeDevice();
    spdlog::info("Pipeline execution complete");

    saveOutput();
}

void Renderer::saveOutput() {
    image_.download();
    spdlog::debug("Image downloaded from device");

    std::span<const float3> framebuffer(image_.host(), image_.size());
    try_unwrap_or_exit(
        io::saveExrImage(framebuffer, image_.width(), image_.height(), config_.output_path_));

    spdlog::info("Image saved to '{}'", config_.output_path_.string());
}

}  // namespace thesis
