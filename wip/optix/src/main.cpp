#include "thesis/cuda/context_handle.h"
#include "thesis/cuda/stream_handle.h"
#include "thesis/host/camera.h"
#include "thesis/host/environment_map.h"
#include "thesis/host/image.h"
#include "thesis/optix/gas_handle.h"
#include "thesis/optix/sbt_handle.h"
#include "thesis/optix/handle.h"
#include "thesis/optix/launch_params.h"
#include "thesis/optix/logging.h"
#include "thesis/optix/record.h"
#include "thesis/pch.h"
#include "thesis/app_config.h"
#include "thesis/utils/check.h"
#include "thesis/utils/io.h"

#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <vector_types.h>

#include <array>
#include <cstddef>
#include <glm/glm.hpp>
#include <optional>
#include <span>
#include <spdlog/spdlog.h>
#include <string>

namespace tcuda = thesis::cuda;
namespace toptix = thesis::optix;
namespace tio = thesis::io;
namespace thost = thesis::host;
namespace tdevice = thesis::device;

auto getConfig(int argc, char* argv[]) {
    thesis::AppConfig config;
    if (auto err = config.parse(argc, argv)) {
        const auto& [code, msg] = *err;
        spdlog::error("Error parsing app arguments: {}", msg);
        std::exit(code);
    }
    return config;
}

int main(int argc, char* argv[]) {
    auto config = getConfig(argc, argv);


    // Parse arguments
    CLI::App app{"OptiX-based raytracer of kernel mixture models"};

    CLI11_PARSE(app, argc, argv);

    // Initialize logger
#ifdef DEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif  // DEBUG
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("Starting OptiX application");
    spdlog::info("Output image path: {}", config.output_path_);

    // Initialize CUDA and OptiX
    const tcuda::ContextHandle ctx(0);
    spdlog::debug("CUDA context initialized");

    OPTIX_CHECK(optixInit());
    spdlog::debug("OptiX initialized");

    // Device context
    OptixDeviceContextOptions dco = {};
    dco.logCallbackFunction = &toptix::contextLogCb;
    dco.logCallbackLevel = static_cast<int>(toptix::LogLevel::Warning);
#ifdef DEBUG
    dco.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
#endif  // DEBUG
    toptix::DeviceContextHandle context(dco, ctx.get());
    spdlog::debug("Optix device context created");

    // Load PTX
    auto ptx = tio::readFileToString(config.ptx_path_);
    if (!ptx) {
        return 1;
    }
    spdlog::info("PTX loaded ({} bytes)", ptx->size());

    // Module
    OptixModuleCompileOptions mco = {};

    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = "params";
    pco.numPayloadValues = 3;

    const toptix::ModuleHandle module(context.get(), mco, pco, *ptx);
    spdlog::debug("OptiX module created");

    // Raygen program group
    OptixProgramGroupDesc raygen_desc = {};
    raygen_desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    raygen_desc.raygen.module = module.get();
    raygen_desc.raygen.entryFunctionName = "__raygen__rg";

    toptix::ProgramGroupHandle raygen_pg(context.get(), raygen_desc);
    spdlog::debug("Raygen program group created");

    // Miss program group
    OptixProgramGroupDesc miss_desc = {};
    miss_desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    miss_desc.miss.module = module.get();
    miss_desc.miss.entryFunctionName = "__miss__ms";

    toptix::ProgramGroupHandle miss_pg(context.get(), miss_desc);
    spdlog::debug("Miss program group created");

    // Closest hit program group
    OptixProgramGroupDesc hitgroup_desc = {};
    hitgroup_desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    hitgroup_desc.hitgroup.moduleCH = module.get();
    hitgroup_desc.hitgroup.entryFunctionNameCH = "__closesthit__ch";

    toptix::ProgramGroupHandle hitgroup_pg(context.get(), hitgroup_desc);
    spdlog::debug("Hitgroup program group created");

    // Pipeline
    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;

    std::array<OptixProgramGroup, 3> program_groups = {raygen_pg.get(), miss_pg.get(),
                                                       hitgroup_pg.get()};
    toptix::PipelineHandle pipeline(context.get(), pco, plo, program_groups.data(),
                                    program_groups.size());
    spdlog::info("OptiX pipeline built");

    // Shader Binding Table
    toptix::Record<void> raygen_record(raygen_pg.get());
    toptix::Record<void> miss_record(miss_pg.get());
    toptix::Record<void> hitgroup_record(hitgroup_pg.get());
    toptix::SBTHandle sbt(raygen_pg.get(), miss_pg.get(), hitgroup_pg.get());
    spdlog::debug("Shader binding table prepared");

    tcuda::StreamHandle stream;

    // Create a simple triangle geometry // TODO(kacper): add more triangles
    const std::array<float3, 3> vertices = {
    float3{-0.5f, -0.5f, -1.0f},
    float3{ 0.5f, -0.5f, -1.0f},
    float3{ 0.0f,  0.5f, -1.0f}
    };

    // Create GAS
    toptix::TriangleGAS gas(context.get(), vertices, stream.get());
    tcuda::StreamHandle::synchronizeDevice();

    // Create host-side environment map
    thost::EnvironmentMap host_env_map(config.env_map_path_);
    
    // Create host-side image
    const size_t width = config.image_width_;  // TODO(kacper): tweak size
    const float aspect_ratio = config.aspect_ratio_;
    thost::Image host_image(width, aspect_ratio);

    // Create host-side camera
    thost::Camera host_camera; // TODO(kacper): is it not a bit redundant to pass the sizes again>
    host_camera.aspect_ratio_ = host_image.aspect_ratio();
    host_camera.image_width_ = host_image.width();
    host_camera.vertical_fov_ = 90.0f;
    host_camera.lookfrom_ = glm::vec3(0.0f, 0.0f, 0.0f);
    host_camera.lookat_ = glm::vec3(0.0f, 0.0f, -1.0f);
    host_camera.vup_ = glm::vec3(0.0f, 1.0f, 0.0f);
    host_camera.build();

    // Set launch parameters
    toptix::LaunchParams params = {};
    params.gas_handle_ = gas.get();
    params.num_samples_per_pixel_ = 10;  // TODO(kacper): tweak
    params.image_ = host_image.toDevice();
    params.env_map_ = host_env_map.toDevice();
    params.camera_ = host_camera.toDevice();

    auto d_params = tcuda::Buffer<decltype(params)>::onDeviceOnly(&params, 1);
    spdlog::debug("Launch parameters uploaded");

    // Launch
    spdlog::info("Launching OptiX pipeline...");
    pipeline.launch(stream.get(), reinterpret_cast<CUdeviceptr>(d_params.device()), sizeof(params), sbt.get(), static_cast<unsigned int>(host_image.width()), static_cast<unsigned int>(host_image.height()));
    tcuda::StreamHandle::synchronizeDevice();
    spdlog::info("Pipeline execution complete");

    // Readback
    host_image.download();
    spdlog::debug("Image downloaded from device");

    // Save as EXR
    std::span<const float3> framebuffer(host_image.host(), host_image.size());
    tio::saveExrImage(framebuffer, host_image.width(), host_image.height(), config.output_path_);
    spdlog::info("Image saved to '{}'", config.output_path_);

    return 0;
}
