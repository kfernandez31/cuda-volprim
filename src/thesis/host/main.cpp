#include "thesis/pch.h"

#include "thesis/host/cuda/context.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/optix/context.h"
#include "thesis/host/optix/module.h"
#include "thesis/host/optix/program_group.h"
#include "thesis/host/optix/pipeline.h"
#include "thesis/host/optix/sbt.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/result.h"
#include "thesis/common/params/launch_params.h"
#include "thesis/device/payloads/base.h"
#include "thesis/host/params/camera.h"
#include "thesis/host/params/environment_map.h"
#include "thesis/host/params/image.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/app/logging.h"

#include <optix_function_table_definition.h>  // important - do not remove or include in another file!

#include <spdlog/spdlog.h>
#include <utility>

using namespace thesis;
using namespace thesis::host;
using namespace thesis::common;
using namespace thesis::device;

struct MinimalLaunchParams {
    OptixTraversableHandle handle;
    float3* image_buffer;
    size_t width;
    size_t height;
};

int main() {
    app::logging::init();
    spdlog::info("Starting minimal sphere test");

    // Cuda context
    cuda::Context cuda_ctx;
    auto stream = std::make_shared<cuda::Stream>(true); // default stream

    // OptiX context
    optix::Context optix_ctx(cuda_ctx.get());

    // Module
    OptixPipelineCompileOptions pco = {};
    pco.pipelineLaunchParamsVariableName = "launch_params";
    pco.numPayloadValues = thesis::device::payloads::MAX_PAYLOADS_IN_USE;
    pco.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS; // | OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING
    pco.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_SPHERE;
    auto module = thesis::host::utils::try_unwrap_or_exit<optix::Module>(
        optix::Module::load(optix_ctx.get(), "build/device_program.optixir", pco)
    );

    // Prepare sphere
    cuda::AsyncBuffer<float3> centers(1, cuda_ctx.get(), stream, cuda::AllocType::OnBoth);
    centers[0] = make_float3(0.0f, 0.0f, 0.0f); // origin-centered
    centers.upload();
    CUdeviceptr vertex_ptr = centers.cu_device_ptr();

    cuda::AsyncBuffer<float> radii(1, cuda_ctx.get(), stream, cuda::AllocType::OnBoth);
    radii[0] = 1.0f; // unit radius
    radii.upload();
    CUdeviceptr radius_ptr = radii.cu_device_ptr();
    
    spdlog::info("Sphere buffers: vertex=0x{:x}, radius=0x{:x}", vertex_ptr, radius_ptr);

    // Prepare build input
    static constexpr unsigned geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

    OptixBuildInput in{}; // zero-initialized
    in.type = OPTIX_BUILD_INPUT_TYPE_SPHERES;
    in.sphereArray.vertexBuffers = &vertex_ptr;
    in.sphereArray.numVertices = 1;
    in.sphereArray.radiusBuffers = &radius_ptr;
    in.sphereArray.flags = geomFlags;
    in.sphereArray.numSbtRecords = 1;

    // Build GAS
    OptixAccelBuildOptions opts{};
    opts.buildFlags = 0;
    opts.operation = OPTIX_BUILD_OPERATION_BUILD;

    OptixAccelBufferSizes sz{};
    OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx.get(), &opts, &in, 1, &sz));
    
    spdlog::info("GAS buffer sizes: temp={}, output={}", sz.tempSizeInBytes, sz.outputSizeInBytes);

    cuda::AsyncBuffer<std::byte> temp(sz.tempSizeInBytes, cuda_ctx.get(), stream, cuda::AllocType::OnDeviceOnly);
    cuda::AsyncBuffer<std::byte> out(sz.outputSizeInBytes, cuda_ctx.get(), stream, cuda::AllocType::OnDeviceOnly);
    
    OptixTraversableHandle gas_handle = 0;
    OPTIX_CHECK(optixAccelBuild(optix_ctx.get(), stream->get(), &opts, &in, 1,
                                temp.cu_device_ptr(), temp.size(), out.cu_device_ptr(),
                                out.size(), &gas_handle, nullptr, 0));
    
    spdlog::info("GAS built with handle: 0x{:x}", gas_handle);

    // Create instance
    // cuda::AsyncBuffer<OptixInstance> instances(1, cuda_ctx.get(), stream, cuda::AllocType::OnBoth);
    // OptixInstance& inst = instances[0];
    
    // Identity transform
    // std::memset(inst.transform, 0, sizeof(inst.transform));
    // inst.transform[0] = inst.transform[5] = inst.transform[10] = 1.0f;
    
    // inst.traversableHandle = gas_handle;
    // inst.instanceId = 0;
    // inst.sbtOffset = 0;
    // inst.visibilityMask = 0xFF;
    // inst.flags = OPTIX_INSTANCE_FLAG_NONE;
    
    // instances.upload();

    // Build IAS
    // OptixBuildInput ias_in{};
    // ias_in.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    // ias_in.instanceArray.instances = instances.cu_device_ptr();
    // ias_in.instanceArray.numInstances = 1;

    // OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx.get(), &opts, &ias_in, 1, &sz));
    // spdlog::info("IAS buffer sizes: temp={}, output={}", sz.tempSizeInBytes, sz.outputSizeInBytes);

    // cuda::AsyncBuffer<std::byte> ias_temp(sz.tempSizeInBytes, cuda_ctx.get(), stream, cuda::AllocType::OnDeviceOnly);
    // cuda::AsyncBuffer<std::byte> ias_out(sz.outputSizeInBytes, cuda_ctx.get(), stream, cuda::AllocType::OnDeviceOnly);
    
    // OptixTraversableHandle ias_handle = 0;
    // OPTIX_CHECK(optixAccelBuild(optix_ctx.get(), nullptr, &opts, &ias_in, 1,
    //                             ias_temp.cu_device_ptr(), ias_temp.size(), ias_out.cu_device_ptr(),
    //                             ias_out.size(), &ias_handle, nullptr, 0));
    
    // spdlog::info("IAS built with handle: 0x{:x}", ias_handle);

    // Create program groups
    auto raygen_pg = optix::ProgramGroup::createRaygen(optix_ctx.get(), module.get(), "__raygen__rg");
    auto miss_pg = optix::ProgramGroup::createMiss(optix_ctx.get(), module.get(), "__miss__ms");
    auto hitgroup_pg = optix::ProgramGroup::createHitgroup(optix_ctx.get(), module.get(), "__closesthit__ch");

    // Build SBT
    optix::SBT sbt(cuda_ctx.get(), stream);
    sbt.build(raygen_pg.get(), miss_pg.get(), hitgroup_pg.get());

    // Create pipeline
    OptixPipelineLinkOptions plo{};
    plo.maxTraceDepth = 1;
    
    std::array<OptixProgramGroup, 3> pgs = {raygen_pg.get(), miss_pg.get(), hitgroup_pg.get()};
    optix::Pipeline pipeline(optix_ctx.get(), pco, plo, pgs.data(), pgs.size());

    // Setup launch params using your existing structure
    cuda::AsyncBuffer<thesis::common::params::LaunchParams> launch_params(1, cuda_ctx.get(), stream, cuda::AllocType::OnBoth);
    auto& params = launch_params[0];
    
    // Initialize minimal params
    params.ias_handle_ = gas_handle;
    params.debug_ = true;
    params.seed_ = 42;
    
    // Setup image
    host::params::Image image(512, 512, 1, cuda_ctx.get(), stream, stream);
    params.image_ = image.toDevice();
    
    // Setup camera
    auto camera = host::params::Camera::getDefaultCamera(512, 512);
    params.camera_ = camera.toDevice();
    
    // Setup primitives
    host::cuda::AsyncBuffer<device::params::Primitive> primitives(1, cuda_ctx.get(), stream, cuda::AllocType::OnBoth);
    primitives[0].albedo_ = make_float3(1.0f, 0.0f, 0.0f); // Red sphere
    primitives.upload();
    params.primitives_ = device::utils::DynamicVector<device::params::Primitive>(primitives.device(), 1);
    
    launch_params.upload();

    // Launch!
    spdlog::info("Launching OptiX pipeline...");
    pipeline.launch(stream->get(), launch_params.cu_device_ptr(),
                    sizeof(common::params::LaunchParams), sbt.get(),
                    512, 512, 1);
    
    CUDA_CHECK(cudaDeviceSynchronize());
    spdlog::info("Pipeline execution complete");

    // Save image
    host::utils::try_unwrap_or_exit(image.save("minimal_sphere_output.exr"));
    
    spdlog::info("Test complete - check minimal_sphere_output.exr");

    return 0;
}