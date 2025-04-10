#include "thesis/cuda_buffer.h"
#include "thesis/cuda_context.h"
#include "thesis/cuda_upload.h"
#include "thesis/optix_handle.h"
#include "thesis/optix_logging.h"
#include "thesis/optix_record.h"
#include "thesis/file_utils.h"

#include <CLI11/CLI11.h>
#include <spdlog/spdlog.h>

#include <optix.h>
#include <cuda_runtime.h>

#include <vector>
#include <algorithm>

static std::string get_ptx_path() {
#ifdef PTX_PATH
    return PTX_PATH;
#else
    throw std::runtime_error("PTX_PATH not defined");
#endif
}

using namespace thesis;

int main(int argc, char* argv[]) {
    // Parse arguments
    CLI::App app{"OptiX-based raytracer of kernel mixture models"};
    
    std::string output_path = "output.exr";
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
    CUDA_CHECK(cudaFree(0));
    spdlog::debug("CUDA context initialized");

    OPTIX_CHECK(optixInit());
    spdlog::debug("OptiX initialized");

    // Device context
    OptixDeviceContextOptions dco = {};
    dco.logCallbackFunction       = &context_log_cb;
    dco.logCallbackLevel          = OPTIX_LOG_LEVEL_WARNING;
#ifdef DEBUG
    dco.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
#endif // DEBUG
    OptixDeviceContextHandle context(dco);
    spdlog::debug("Optix device context created");

    // Load PTX
    auto ptx = read_file_to_string(get_ptx_path());
    if (!ptx) return 1;
    spdlog::info("PTX loaded ({} bytes)", ptx->size());

    // Module
    OptixModuleCompileOptions mco = {};
    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = "optixLaunchParams";
    OptixModuleHandle module(context.get(), mco, pco, *ptx);
    spdlog::debug("OptiX module created");

    // Raygen program group
    OptixProgramGroupDesc raygenDesc = {};
    raygenDesc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    raygenDesc.raygen.module = module.get();
    raygenDesc.raygen.entryFunctionName = "__raygen__hello";
    OptixProgramGroupHandle raygenPG(context.get(), raygenDesc);
    spdlog::debug("Raygen program group created");

    OptixProgramGroupDesc missDesc = {};
    missDesc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    missDesc.miss.module = module.get();
    missDesc.miss.entryFunctionName = "__miss__noop";
    OptixProgramGroupHandle missPG(context.get(), missDesc);
    spdlog::debug("Miss program group created");

    // Pipeline
    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;

    std::array<OptixProgramGroup, 2> program_groups = { raygenPG.get(), missPG.get() };
    OptixPipelineHandle pipeline(context.get(), pco, plo, program_groups.data(), static_cast<unsigned int>(program_groups.size()));
    spdlog::info("OptiX pipeline built");

    // Shader Binding Table
    OptixRecord<void> raygenRecord(raygenPG.get());
    OptixRecord<void> missRecord(missPG.get());

    OptixShaderBindingTable sbt = {};
    sbt.raygenRecord = raygenRecord.get();
    sbt.missRecordBase          = missRecord.get();
    sbt.missRecordStrideInBytes = OPTIX_SBT_RECORD_HEADER_SIZE;
    sbt.missRecordCount         = 1;

    spdlog::debug("raygenRecord ptr: {}", static_cast<uint64_t>(raygenRecord.get()));
    spdlog::debug("missRecord ptr: {}", static_cast<uint64_t>(missRecord.get()));
    spdlog::debug("Shader binding table prepared");

    // Allocate output buffer
    const size_t width = 512;
    const size_t height = 384;
    CudaBuffer<float4> buffer(width * height);
    spdlog::info("Output buffer allocated ({}x{})", width, height);

    // Set launch parameters
    struct LaunchParams { float4* output_buffer; };
    LaunchParams params = { buffer.device() };
    CudaUpload<LaunchParams> d_params(params);
    spdlog::debug("Launch parameters uploaded");

    CUstream stream;
    CUDA_CHECK(cudaStreamCreate(&stream));

    // Launch
    spdlog::info("Launching OptiX pipeline...");
    OPTIX_CHECK(optixLaunch(pipeline.get(), stream, d_params, sizeof(LaunchParams), &sbt, width, height, 1));
    CUDA_CHECK(cudaDeviceSynchronize());
    spdlog::info("Kernel execution complete");

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
    thesis::save_exr_image(framebuffer, width, height, std::string(output_path));
    spdlog::info("Image saved to '{}'", output_path);

    return 0;
}
