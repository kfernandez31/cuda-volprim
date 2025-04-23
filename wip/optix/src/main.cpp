#include "thesis/cuda/buffer.h"
#include "thesis/cuda/context_handle.h"
#include "thesis/cuda/stream_handle.h"
#include "thesis/cuda/upload_buffer.h"
#include "thesis/optix/handle.h"
#include "thesis/optix/launch_params.h"
#include "thesis/optix/logging.h"
#include "thesis/optix/record.h"
#include "thesis/utils/check.h"
#include "thesis/utils/io.h"

#include <third_party/CLI11/CLI11.h>
#include <third_party/spdlog/spdlog.h>

#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <vector_types.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace {

std::string getPtxPath() {
#ifdef PTX_PATH
    return PTX_PATH;
#else
    throw std::runtime_error("PTX_PATH not defined");
#endif
}

}  // namespace

namespace tcuda = thesis::cuda;
namespace toptix = thesis::optix;
namespace tio = thesis::io;

int main(int argc, char* argv[]) {
    // Parse arguments
    CLI::App app{"OptiX-based raytracer of kernel mixture models"};

    std::string output_path("output.exr");
    app.add_option("-o,--output", output_path, "Path to save the rendered image")->required(false);

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
    auto ptx = tio::readFileToString(getPtxPath());
    if (!ptx) {
        return 1;
    }
    spdlog::info("PTX loaded ({} bytes)", ptx->size());

    // Module
    OptixModuleCompileOptions mco = {};

    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = "optixLaunchParams";

    const toptix::ModuleHandle module(context.get(), mco, pco, *ptx);
    spdlog::debug("OptiX module created");

    // Raygen program group
    OptixProgramGroupDesc raygen_desc = {};
    raygen_desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    raygen_desc.raygen.module = module.get();
    raygen_desc.raygen.entryFunctionName = "__raygen__hello";

    toptix::ProgramGroupHandle raygen_pg(context.get(), raygen_desc);
    spdlog::debug("Raygen program group created");

    // Miss program group
    OptixProgramGroupDesc miss_desc = {};
    miss_desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    miss_desc.miss.module = module.get();
    miss_desc.miss.entryFunctionName = "__miss__noop";

    toptix::ProgramGroupHandle miss_pg(context.get(), miss_desc);
    spdlog::debug("Miss program group created");

    // Pipeline
    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;

    std::array<OptixProgramGroup, 2> program_groups = {raygen_pg.get(), miss_pg.get()};
    toptix::PipelineHandle pipeline(context.get(), pco, plo, program_groups.data(),
                                    static_cast<unsigned int>(program_groups.size()));
    spdlog::info("OptiX pipeline built");

    // Shader Binding Table
    toptix::Record<void> raygen_record(raygen_pg.get());
    toptix::Record<void> miss_record(miss_pg.get());

    OptixShaderBindingTable sbt = {};
    sbt.raygenRecord = raygen_record.get();
    sbt.missRecordBase = miss_record.get();
    sbt.missRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
    sbt.missRecordCount = 1;

    spdlog::debug("Shader binding table prepared");

    // Allocate output buffer
    const size_t width = 512;
    const size_t height = 384;
    tcuda::Buffer<float4> buffer(width * height);
    spdlog::info("Output buffer allocated ({}x{})", width, height);

    // Set launch parameters
    const toptix::LaunchParams params = {buffer.device()};
    tcuda::UploadBuffer<toptix::LaunchParams> d_params(params);
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

    // Prepare EXR data
    std::vector<float3> framebuffer(buffer.size());
    std::transform(buffer.host(), buffer.host() + buffer.size(), framebuffer.begin(),
                   [](const auto& px) { return make_float3(px.x, px.y, px.z); });
    spdlog::debug("Framebuffer prepared for EXR output");

    // Save as EXR
    tio::saveExrImage(framebuffer, width, height, output_path);
    spdlog::info("Image saved to '{}'", output_path);

    return 0;
}
