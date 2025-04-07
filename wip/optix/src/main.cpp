#include <optix.h>
#include <cuda_runtime.h>

#include <vector>
#include <algorithm>

#include "thesis/cuda_buffer.h"
#include "thesis/cuda_context.h"
#include "thesis/cuda_upload.h"
#include "thesis/optix_handle.h"
#include "thesis/optix_record.h"
#include "thesis/image_io.h"

using namespace thesis;

// TODO: add logging
int main(int argc, char* argv[]) {
    const std::string_view output_path = (argc > 1) ? argv[1] : "output.exr";

    // Initialize CUDA and OptiX
    CUDA_CHECK(cudaFree(0));
    OPTIX_CHECK(optixInit());

    // Device context
    OptixDeviceContextHandle context;

    // Load PTX
    auto ptx = read_ptx("device_program.ptx");

    // Module
    OptixModuleCompileOptions mco = {};
    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = "optixLaunchParams";
    OptixModuleHandle module(context.get(), mco, pco, ptx);

    // Raygen program group
    OptixProgramGroupDesc pgDesc = {};
    pgDesc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    pgDesc.raygen.module = module.get();
    pgDesc.raygen.entryFunctionName = "__raygen__hello";
    OptixProgramGroupHandle raygenPG(context.get(), pgDesc);

    // Pipeline
    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;
    OptixPipelineHandle pipeline(context.get(), pco, plo, &raygenPG.get(), 1);

    // Shader Binding Table
    OptixRecord<void> raygenRecord(raygenPG.get());
    OptixShaderBindingTable sbt = {};
    sbt.raygenRecord = raygenRecord;

    // Allocate output buffer
    const size_t width = 512;
    const size_t height = 384;
    CudaBuffer<float4> buffer(width * height);

    // Set launch parameters
    struct LaunchParams { float4* output_buffer; };
    LaunchParams params = { buffer.device() };
    CudaUpload<LaunchParams> d_params(params);

    // Launch
    OPTIX_CHECK(optixLaunch(pipeline.get(), 0, d_params, sizeof(LaunchParams), &sbt, width, height, 1));
    CUDA_CHECK(cudaDeviceSynchronize());

    // Readback
    buffer.download();

    // Prepare EXR data
    std::vector<float3> framebuffer(buffer.size());
    std::transform(buffer.host(), buffer.host() + buffer.size(), framebuffer.begin(),
        [](const auto& px) { return make_float3(px.x, px.y, px.z); }
    );

    // Save as EXR
    thesis::save_exr_image(framebuffer, width, height, output_path); // TODO: pull from argv[1]

    return 0;
}
