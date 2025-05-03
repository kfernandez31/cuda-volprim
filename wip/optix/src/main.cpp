#include "thesis/cuda/context_handle.h"
#include "thesis/cuda/stream_handle.h"
#include "thesis/cuda/upload_buffer.h"
#include "thesis/host/camera.h"
#include "thesis/host/environment_map.h"
#include "thesis/host/image.h"
#include "thesis/optix/gas_handle.h"
#include "thesis/optix/handle.h"
#include "thesis/optix/launch_params.h"
#include "thesis/optix/logging.h"
#include "thesis/optix/record.h"
#include "thesis/pch.h"
#include "thesis/utils/check.h"
#include "thesis/utils/io.h"

#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <vector_types.h>

#include <CLI11/CLI11.h>
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

int main(int argc, char* argv[]) {
    // Parse arguments
    CLI::App app{"OptiX-based raytracer of kernel mixture models"};

    std::string output_path("output.exr");
    app.add_option("-o,--output", output_path, "Path to save the rendered image")->required(false);

    std::string ptx_path("build/device_program.ptx");
    app.add_option("-p,--ptx", ptx_path, "Path to the PTX file")->required(false);

    std::string env_map_path = "assets/meadow_2_4k.hdr";  // TODO(kacper): system-generic path
    app.add_option("-e,--env_map", env_map_path, "Path to the environment map")->required(false);

    CLI11_PARSE(app, argc, argv);

    // Initialize logger
#ifdef DEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif  // DEBUG
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("Starting OptiX application");
    spdlog::info("Output image path: {}", output_path);

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
    toptix::DeviceContextHandle context(dco);
    spdlog::debug("Optix device context created");

    // Load PTX
    auto ptx = tio::readFileToString(ptx_path);
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
    toptix::PipelineHandle pipeline(context.get(), pco, plo, *program_groups.data(),
                                    program_groups.size());
    spdlog::info("OptiX pipeline built");

    // Shader Binding Table
    toptix::Record<void> raygen_record(raygen_pg.get());
    toptix::Record<void> miss_record(miss_pg.get());
    toptix::Record<void> hitgroup_record(hitgroup_pg.get());

    OptixShaderBindingTable sbt = {};
    sbt.raygenRecord = raygen_record.get();
    sbt.missRecordBase = miss_record.get();
    sbt.missRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
    sbt.missRecordCount = 1;
    sbt.hitgroupRecordBase = hitgroup_record.get();
    sbt.hitgroupRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
    sbt.hitgroupRecordCount = 1;

    spdlog::debug("Shader binding table prepared");

    // Create a simple triangle geometry // TODO(kacper): add more triangles
    const std::array<float3, 3> vertices = {float3{-0.5f, -0.5f, 0.0f}, float3{0.5f, -0.5f, 0.0f},
                                            float3{0.0f, 0.5f, 0.0f}};

    // Create GAS
    toptix::TriangleGAS gas(context.get(), vertices);

    // Create host-side environment map
    const thost::EnvironmentMap host_env_map(env_map_path);

    // Create host-side image
    constexpr size_t width = 512;  // TODO(kacper): tweak size
    constexpr float aspect_ratio = 16.0f / 9.0f;
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
    params.handle_ = gas.get();
    params.num_samples_per_pixel_ = 10;  // TODO(kacper): tweak
    params.image_ = host_image.toDevice();
    params.env_map_ = host_env_map.toDevice();
    params.camera_ = host_camera.toDevice();

    tcuda::UploadBuffer<decltype(params)> d_params(params);
    spdlog::debug("Launch parameters uploaded");

    tcuda::StreamHandle stream;

    // Launch
    spdlog::info("Launching OptiX pipeline...");
    pipeline.launch(stream.get(), d_params.get(), sizeof(params), sbt, static_cast<unsigned int>(host_image.width()), static_cast<unsigned int>(host_image.height()));
    tcuda::StreamHandle::synchronizeDevice();
    spdlog::info("Pipeline execution complete");

    // Readback
    host_image.download();
    spdlog::debug("Image downloaded from device");

    // Save as EXR
    std::span<const float3> framebuffer(host_image.host(), host_image.size());
    tio::saveExrImage(framebuffer, host_image.width(), host_image.height(), output_path);
    spdlog::info("Image saved to '{}'", output_path);

    return 0;
}
