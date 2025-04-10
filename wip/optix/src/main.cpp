#include <optix.h>
#include <cuda_runtime.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <algorithm>

#include "thesis/cuda_buffer.h"
#include "thesis/cuda_context.h"
#include "thesis/cuda_upload.h"
#include "thesis/optix_handle.h"
#include "thesis/optix_record.h"
#include "thesis/image_io.h"

static std::string get_ptx_path() {
#ifdef PTX_PATH
    return PTX_PATH;
#else
    throw std::runtime_error("PTX_PATH not defined");
#endif
}

using namespace thesis;

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug); // or info/warn/error
    // spdlog::set_pattern("[%T] [%^%l%$] %v"); // if you want timestamps/colors

    const std::string_view output_path = (argc > 1) ? argv[1] : "output.exr";

    spdlog::info("Starting OptiX application");
    spdlog::info("Output image path: {}", output_path);

    // Initialize CUDA and OptiX
    CUDA_CHECK(cudaFree(0));
    spdlog::debug("CUDA context initialized");

    OPTIX_CHECK(optixInit());
    spdlog::debug("OptiX initialized");

    // Device context
    OptixDeviceContextHandle context;
    spdlog::debug("Optix device context created");

    // Load PTX
    std::string ptx;
    try {
        ptx = read_ptx(get_ptx_path());
        spdlog::info("PTX loaded ({} bytes)", ptx.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to load PTX: {}", e.what());
        return 1;
    }

    // Module
    OptixModuleCompileOptions mco = {};
    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = "optixLaunchParams";
    OptixModuleHandle module(context.get(), mco, pco, ptx);
    spdlog::debug("OptiX module created");

    // Raygen program group
    OptixProgramGroupDesc pgDesc = {};
    pgDesc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    pgDesc.raygen.module = module.get();
    pgDesc.raygen.entryFunctionName = "__raygen__hello";

    OptixProgramGroupHandle raygenPG(context.get(), pgDesc);
    spdlog::debug("Raygen program group created");

    // Pipeline
    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;
    OptixPipelineHandle pipeline(context.get(), pco, plo, &raygenPG.get(), 1);
    spdlog::info("OptiX pipeline built");

    // Shader Binding Table
    OptixRecord<void> raygenRecord(raygenPG.get());
    OptixShaderBindingTable sbt = {};
    sbt.raygenRecord = raygenRecord.get();
    assert(sbt.raygenRecord != 0);
    spdlog::debug("raygenRecord ptr: {}", static_cast<uint64_t>(raygenRecord.get()));
    spdlog::debug("Shader binding table prnepared");

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

    // Launch
    spdlog::info("Launching OptiX pipeline...");
    OPTIX_CHECK(optixLaunch(pipeline.get(), 0, d_params, sizeof(LaunchParams), &sbt, width, height, 1));
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
    thesis::save_exr_image(framebuffer, width, height, output_path);
    spdlog::info("Image saved to '{}'", output_path);

    return 0;
}
