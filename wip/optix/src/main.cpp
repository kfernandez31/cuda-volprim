// TODO: includes

#include <optix.h>
#include <cuda_runtime.h>

#include <vector>
#include <algorithm>

#include "cuda_buffer.h"
#include "cuda_context.h"
#include "cuda_upload.h"
#include "optix_handle.h"
#include "optix_record.h"
#include "image_io.h" // for read_ptx + assume save_exr_image()

// TODO: add logging
int main() {
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
    OptixModuleHandle module(context, mco, pco, ptx);

    // Raygen program group
    OptixProgramGroupDesc pgDesc = {};
    pgDesc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    pgDesc.raygen.module = module;
    pgDesc.raygen.entryFunctionName = "__raygen__hello";
    OptixProgramGroupHandle raygenPG(context, pgDesc);

    // Pipeline
    OptixPipelineLinkOptions plo = {};
    plo.maxTraceDepth = 1;
    OptixPipelineHandle pipeline(context, pco, plo, &raygenPG, 1);

    // Shader Binding Table
    OptixRecord<void> raygenRecord(raygenPG);
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
    OPTIX_CHECK(optixLaunch(pipeline, 0, d_params, sizeof(LaunchParams), &sbt, width, height, 1));
    CUDA_CHECK(cudaDeviceSynchronize());

    // Readback
    buffer.download();

    // Prepare EXR data
    std::vector<vec3> framebuffer(buffer.size());
    std::transform(buffer.host(), buffer.host() + buffer.size(), framebuffer.begin(),
        [](const vec3& px) { return make_float3(px.x, px.y, px.z); }
    );

    // Save as EXR
    save_exr_image(framebuffer, width, height, output_path); // TODO: pull from argv[1]

    return 0;
}
