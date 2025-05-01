#include "thesis/pch.h"

#include "thesis/cuda/buffer.h"
#include "thesis/cuda/context_handle.h"
#include "thesis/cuda/stream_handle.h"
#include "thesis/cuda/upload_buffer.h"
#include "thesis/optix/gas_handle.h"
#include "thesis/optix/handle.h"
#include "thesis/optix/launch_params.h"
#include "thesis/optix/logging.h"
#include "thesis/optix/record.h"
#include "thesis/utils/check.h"
#include "thesis/utils/io.h"
#include "thesis/host/environment_map.h"
#include "thesis/host/camera.h"

#include <CLI11/CLI11.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <vector_types.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>
#include <span>

namespace tcuda = thesis::cuda;
namespace toptix = thesis::optix;
namespace tio = thesis::io;
namespace thost = thesis::host;

int main(int argc, char* argv[]) {
    // Parse arguments
    CLI::App app{"OptiX-based raytracer of kernel mixture models"};

    std::string output_path("output.exr");
    app.add_option("-o,--output", output_path, "Path to save the rendered image")->required(false);

    std::string ptx_path("build/device_program.ptx");
    app.add_option("-p,--ptx", ptx_path, "Path to the PTX file")->required(false);

    std::string env_map_path = "assets/meadow_2_4k.hdr"; // TODO: system-generic path
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

    std::array<OptixProgramGroup, 3> program_groups = {raygen_pg.get(), miss_pg.get(), hitgroup_pg.get()};
    toptix::PipelineHandle pipeline(context.get(), pco, plo, *program_groups.data(), program_groups.size());
    spdlog::info("OptiX pipeline built");

    // Shader Binding Table
    toptix::Record<void> raygen_record(raygen_pg.get());
    toptix::Record<void> miss_record(miss_pg.get());
    toptix::Record<void> hitgroup_record(hitgroup_pg.get());

    OptixShaderBindingTable sbt = {};
    sbt.raygenRecord                = raygen_record.get();
    sbt.missRecordBase              = miss_record.get();
    sbt.missRecordStrideInBytes     = OPTIX_SBT_RECORD_HEADER_SIZE;
    sbt.missRecordCount             = 1;
    sbt.hitgroupRecordBase          = hitgroup_record.get();
    sbt.hitgroupRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
    sbt.hitgroupRecordCount         = 1;

    spdlog::debug("Shader binding table prepared");

    // Allocate output buffer
    const size_t width = 512;
    const size_t height = 384;
    tcuda::Buffer<float3> buffer(width * height);
    spdlog::info("Output buffer allocated ({}x{})", width, height);

    // Create a simple triangle geometry //TODO: add more triangles
    const std::array<float3, 3> vertices = {
        float3{-0.5f, -0.5f, 0.0f},
        float3{0.5f, -0.5f, 0.0f},
        float3{0.0f, 0.5f, 0.0f}
    };

    // Create GAS
    toptix::TriangleGAS gas(context.get(), vertices);

    // Create host-side environment map
    thost::EnvironmentMap host_env_map(env_map_path);

    // Create host-side camera
    thost::Camera host_camera;
    host_camera.aspect_ratio = static_cast<float>(width) / height;
    host_camera.image_width  = width;
    host_camera.image_height = height;
    host_camera.vertical_fov = 90.0f;
    host_camera.lookfrom     = glm::vec3(0.0f, 0.0f, 0.0f);
    host_camera.lookat       = glm::vec3(0.0f, 0.0f, -1.0f);
    host_camera.vup          = glm::vec3(0.0f, 1.0f, 0.0f);
    host_camera.build();


    // Set launch parameters
    toptix::LaunchParams params = {};
    params.image = buffer.device();
    params.image_width = width;
    params.image_height = height;
    params.handle = gas.get();
    params.env_map = host_env_map.toDevice();
    params.camera = host_camera.toDevice();

    tcuda::UploadBuffer<decltype(params)> d_params(params);
    spdlog::debug("Launch parameters uploaded");

    tcuda::StreamHandle stream;

    // Launch
    spdlog::info("Launching OptiX pipeline...");
    pipeline.launch(stream.get(), d_params.get(), sizeof(params), sbt, width, height);
    tcuda::StreamHandle::synchronizeDevice();
    spdlog::info("Pipeline execution complete");

    // Readback
    buffer.download();
    spdlog::debug("Buffer downloaded from device");

    
    // Save as EXR
    std::span<const float3> framebuffer(buffer.host(), buffer.size());
    tio::saveExrImage(framebuffer, width, height, output_path);
    spdlog::info("Image saved to '{}'", output_path);

    return 0;
}
