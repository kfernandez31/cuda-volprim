#include "thesis/check.h"
#include "thesis/cuda_buffer.h"
#include "thesis/cuda_context.h"
#include "thesis/cuda_stream.h"
#include "thesis/cuda_upload.h"
#include "thesis/launch_params.h"
#include "thesis/optix_handle.h"
#include "thesis/optix_logging.h"
#include "thesis/optix_record.h"
#include "thesis/file_utils.h"

#include <CLI11/CLI11.h>
#include <spdlog/spdlog.h>

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

} // namespace

// TODO(kacper): reconsider naming and namespacing, maybe grouping some symbols up in sub-namespaces of "thesis"
using thesis::CudaBuffer;
using thesis::CudaContextHandle;
using thesis::CudaStreamHandle;
using thesis::CudaUpload;
using thesis::OptixDeviceContextHandle;
using thesis::OptixModuleHandle;
using thesis::OptixProgramGroupHandle;
using thesis::OptixPipelineHandle;
using thesis::OptixRecord;
using thesis::saveExrImage;
using thesis::contextLogCb;
using thesis::readFileToString;

OptixFunctionTable g_optixFunctionTable_105 = {};

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
#endif // DEBUG
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("Starting OptiX application");
    spdlog::info("Output image path: {}", output_path);

    // Initialize CUDA and OptiX
    const CudaContextHandle ctx(0);
    spdlog::debug("CUDA context initialized");

    OPTIX_CHECK(optixInit());
    spdlog::debug("OptiX initialized");

    // Device context
    OptixDeviceContextOptions dco = {};
    dco.logCallbackFunction       = &contextLogCb;
    dco.logCallbackLevel          = OPTIX_LOG_LEVEL_WARNING;
#ifdef DEBUG
    dco.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
#endif // DEBUG
    OptixDeviceContextHandle context(dco);
    spdlog::debug("Optix device context created");

    // Load PTX
    auto ptx = readFileToString(getPtxPath());
    if (!ptx) {
        return 1;
    }
    spdlog::info("PTX loaded ({} bytes)", ptx->size());

    // Module
    OptixModuleCompileOptions mco = {};

    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = "optixLaunchParams";

    const OptixModuleHandle module(context.get(), mco, pco, *ptx);
    spdlog::debug("OptiX module created");

    // Raygen program group
    OptixProgramGroupDesc raygen_desc = {};
    raygen_desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    raygen_desc.raygen.module = module.get();
    raygen_desc.raygen.entryFunctionName = "__raygen__hello";

    OptixProgramGroupHandle raygen_pg(context.get(), raygen_desc);
    spdlog::debug("Raygen program group created");

    // Miss program group
    OptixProgramGroupDesc miss_desc = {};
    miss_desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    miss_desc.miss.module = module.get();
    miss_desc.miss.entryFunctionName = "__miss__noop";

    OptixProgramGroupHandle miss_pg(context.get(), miss_desc);
    spdlog::debug("Miss program group created");

    // Pipeline
    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;

    std::array<OptixProgramGroup, 2> program_groups = { raygen_pg.get(), miss_pg.get() };
    OptixPipelineHandle pipeline(context.get(), pco, plo, program_groups.data(), static_cast<unsigned int>(program_groups.size()));
    spdlog::info("OptiX pipeline built");

    // Shader Binding Table
    OptixRecord<void> raygen_record(raygen_pg.get());
    OptixRecord<void> miss_record(miss_pg.get());

    OptixShaderBindingTable sbt = {};
    sbt.raygenRecord            = raygen_record.get();
    sbt.missRecordBase          = miss_record.get();
    sbt.missRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
    sbt.missRecordCount         = 1;

    spdlog::debug("Shader binding table prepared");

    // Allocate output buffer
    const size_t width  = 512;
    const size_t height = 384;
    CudaBuffer<float4> buffer(width * height);
    spdlog::info("Output buffer allocated ({}x{})", width, height);

    // Set launch parameters
    const LaunchParams params = { buffer.device() };
    CudaUpload<LaunchParams> d_params(params);
    spdlog::debug("Launch parameters uploaded");

    CudaStreamHandle stream;

    // Launch
    spdlog::info("Launching OptiX pipeline...");
    pipeline.launch(stream.get(), d_params.get(), sizeof(LaunchParams), &sbt, width, height);
    CudaStreamHandle::synchronizeDevice();
    spdlog::info("Pipeline execution complete");

    // Readback
    buffer.download();
    spdlog::debug("Buffer downloaded from device");

    // Prepare EXR data
    std::vector<float3> framebuffer(buffer.size());
    std::transform(buffer.host(), buffer.host() + buffer.size(), framebuffer.begin(),
        [](const auto& px) { return make_float3(px.x, px.y, px.z); }
    );
    spdlog::debug("Framebuffer prepared for EXR output");

    // Save as EXR
    saveExrImage(framebuffer, width, height, output_path);
    spdlog::info("Image saved to '{}'", output_path);

    return 0;
}
