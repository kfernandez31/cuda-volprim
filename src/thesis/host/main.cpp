#include "thesis/pch.h"

#include "thesis/host/cuda/context.h"
#include "thesis/host/cuda/buffer.h"
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
    pco.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS | OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
    pco.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;  // Changed to triangles
    
    // Log OptiX information
    spdlog::info("OptiX primitive flags: TRIANGLE (testing with box mesh instead of sphere)");
    auto module = thesis::host::utils::try_unwrap_or_exit<optix::Module>(
        optix::Module::load(optix_ctx.get(), "build/device_program.optixir", pco)
    );

    // Create a simple box using triangles instead of sphere primitive
    // Box centered at origin with size 2x2x2 (from -1 to 1 on each axis)
    float3 vertices[] = {
        // Front face
        make_float3(-1.0f, -1.0f,  1.0f),  // 0
        make_float3( 1.0f, -1.0f,  1.0f),  // 1
        make_float3( 1.0f,  1.0f,  1.0f),  // 2
        make_float3(-1.0f,  1.0f,  1.0f),  // 3
        // Back face
        make_float3(-1.0f, -1.0f, -1.0f),  // 4
        make_float3( 1.0f, -1.0f, -1.0f),  // 5
        make_float3( 1.0f,  1.0f, -1.0f),  // 6
        make_float3(-1.0f,  1.0f, -1.0f),  // 7
    };
    
    // Define triangles (2 per face, 12 total)
    uint3 indices[] = {
        // Front face
        make_uint3(0, 1, 2), make_uint3(0, 2, 3),
        // Back face
        make_uint3(5, 4, 7), make_uint3(5, 7, 6),
        // Left face
        make_uint3(4, 0, 3), make_uint3(4, 3, 7),
        // Right face
        make_uint3(1, 5, 6), make_uint3(1, 6, 2),
        // Top face
        make_uint3(3, 2, 6), make_uint3(3, 6, 7),
        // Bottom face
        make_uint3(4, 5, 1), make_uint3(4, 1, 0),
    };
    
    cuda::Buffer<float3> vertex_buffer(8, cuda_ctx.get(), cuda::AllocType::OnBoth);
    cuda::Buffer<uint3> index_buffer(12, cuda_ctx.get(), cuda::AllocType::OnBoth);
    
    memcpy(vertex_buffer.host(), vertices, sizeof(vertices));
    memcpy(index_buffer.host(), indices, sizeof(indices));
    
    vertex_buffer.upload();
    index_buffer.upload();
    
    CUdeviceptr vertex_ptr = vertex_buffer.cu_device_ptr();
    CUdeviceptr index_ptr = index_buffer.cu_device_ptr();
    
    spdlog::info("Triangle mesh buffers: vertex=0x{:x}, index=0x{:x}", vertex_ptr, index_ptr);
    spdlog::info("Box mesh: 8 vertices, 12 triangles (centered at origin, size 2x2x2)");

    // Prepare build input for triangles
    static constexpr unsigned geomFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

    OptixBuildInput in{}; // zero-initialized
    in.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    in.triangleArray.vertexBuffers = &vertex_ptr;
    in.triangleArray.numVertices = 8;
    in.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
    in.triangleArray.vertexStrideInBytes = sizeof(float3);
    in.triangleArray.indexBuffer = index_ptr;
    in.triangleArray.numIndexTriplets = 12;
    in.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
    in.triangleArray.indexStrideInBytes = sizeof(uint3);
    in.triangleArray.flags = geomFlags;
    in.triangleArray.numSbtRecords = 1;
    in.triangleArray.preTransform = 0;

    // Build GAS
    OptixAccelBuildOptions opts{};
    opts.buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
    opts.operation = OPTIX_BUILD_OPERATION_BUILD;

    OptixAccelBufferSizes sz{};
    OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx.get(), &opts, &in, 1, &sz));
    
    spdlog::info("GAS buffer sizes: temp={}, output={}", sz.tempSizeInBytes, sz.outputSizeInBytes);

    cuda::Buffer<std::byte> temp(sz.tempSizeInBytes, cuda_ctx.get(), cuda::AllocType::OnDeviceOnly);
    cuda::Buffer<std::byte> out(sz.outputSizeInBytes, cuda_ctx.get(), cuda::AllocType::OnDeviceOnly);
    
    OptixTraversableHandle gas_handle = 0;
    OPTIX_CHECK(optixAccelBuild(optix_ctx.get(), nullptr, &opts, &in, 1,
                                temp.cu_device_ptr(), temp.size(), out.cu_device_ptr(),
                                out.size(), &gas_handle, nullptr, 0));
    
    spdlog::info("GAS built with handle: 0x{:x}", gas_handle);
    spdlog::info("Box GAS created (triangle mesh, centered at origin, size 2x2x2)");
    
    // Validate the GAS was built correctly
    if (gas_handle == 0) {
        spdlog::error("GAS handle is null!");
    }
    
    // Synchronize to ensure build is complete
    CUDA_CHECK(cudaDeviceSynchronize());

    // Create instance
    cuda::Buffer<OptixInstance> instances(1, cuda_ctx.get(), cuda::AllocType::OnBoth);
    OptixInstance& inst = instances[0];
    
    // Identity transform
    std::memset(inst.transform, 0, sizeof(inst.transform));
    inst.transform[0] = inst.transform[5] = inst.transform[10] = 1.0f;
    
    spdlog::info("Instance transform matrix:");
    spdlog::info("  [{:.3f}, {:.3f}, {:.3f}, {:.3f}]", 
                 inst.transform[0], inst.transform[1], inst.transform[2], inst.transform[3]);
    spdlog::info("  [{:.3f}, {:.3f}, {:.3f}, {:.3f}]", 
                 inst.transform[4], inst.transform[5], inst.transform[6], inst.transform[7]);
    spdlog::info("  [{:.3f}, {:.3f}, {:.3f}, {:.3f}]", 
                 inst.transform[8], inst.transform[9], inst.transform[10], inst.transform[11]);
    
    inst.traversableHandle = gas_handle;
    inst.instanceId = 0;
    inst.sbtOffset = 0;
    inst.visibilityMask = 0xFF;
    inst.flags = OPTIX_INSTANCE_FLAG_NONE;
    
    spdlog::info("Instance settings: traversable=0x{:x}, id={}, sbtOffset={}, mask=0x{:x}", 
                 inst.traversableHandle, inst.instanceId, inst.sbtOffset, inst.visibilityMask);
    
    instances.upload();

    // Build IAS
    OptixBuildInput ias_in{};
    ias_in.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    ias_in.instanceArray.instances = instances.cu_device_ptr();
    ias_in.instanceArray.numInstances = 1;

    OPTIX_CHECK(optixAccelComputeMemoryUsage(optix_ctx.get(), &opts, &ias_in, 1, &sz));
    spdlog::info("IAS buffer sizes: temp={}, output={}", sz.tempSizeInBytes, sz.outputSizeInBytes);

    cuda::Buffer<std::byte> ias_temp(sz.tempSizeInBytes, cuda_ctx.get(), cuda::AllocType::OnDeviceOnly);
    cuda::Buffer<std::byte> ias_out(sz.outputSizeInBytes, cuda_ctx.get(), cuda::AllocType::OnDeviceOnly);
    
    OptixTraversableHandle ias_handle = 0;
    OPTIX_CHECK(optixAccelBuild(optix_ctx.get(), nullptr, &opts, &ias_in, 1,
                                ias_temp.cu_device_ptr(), ias_temp.size(), ias_out.cu_device_ptr(),
                                ias_out.size(), &ias_handle, nullptr, 0));
    
    spdlog::info("IAS built with handle: 0x{:x}", ias_handle);

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
    cuda::Buffer<thesis::common::params::LaunchParams> launch_params(1, cuda_ctx.get(), cuda::AllocType::OnBoth);
    auto& params = launch_params[0];
    
    // Initialize minimal params
    params.ias_handle_ = ias_handle;  // Use IAS handle instead of GAS
    params.debug_ = true;
    params.seed_ = 42;
    
    // Setup image
    host::params::Image image(512, 512, 1, cuda_ctx.get(), stream, stream);
    params.image_ = image.toDevice();
    
    // Setup camera
    auto camera = host::params::Camera::getDefaultCamera(512, 512);
    params.camera_ = camera.toDevice();
    
    // Setup primitives
    host::cuda::Buffer<device::params::Primitive> primitives(1, cuda_ctx.get(), cuda::AllocType::OnBoth);
    primitives[0].albedo_ = make_float3(1.0f, 0.0f, 0.0f); // Red sphere
    primitives.upload();
    params.primitives_ = device::utils::DynamicVector<device::params::Primitive>(primitives.device(), 1);
    
    launch_params.upload();

    // First test: Direct GAS traversal
    spdlog::info("Test 1: Launching with direct GAS traversal...");
    params.ias_handle_ = gas_handle;  // Use GAS directly
    launch_params.upload();
    
    // Update pipeline compile options for GAS-only
    OptixPipelineCompileOptions pco_gas = pco;
    pco_gas.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
    
    pipeline.launch(nullptr, launch_params.cu_device_ptr(),
                    sizeof(common::params::LaunchParams), sbt.get(),
                    512, 512, 1);
    
    CUDA_CHECK(cudaDeviceSynchronize());
    spdlog::info("GAS test complete");
    
    // Second test: IAS traversal  
    spdlog::info("Test 2: Launching with IAS traversal...");
    params.ias_handle_ = ias_handle;  // Use IAS
    launch_params.upload();
    
    pipeline.launch(nullptr, launch_params.cu_device_ptr(),
                    sizeof(common::params::LaunchParams), sbt.get(),
                    512, 512, 1);
    
    CUDA_CHECK(cudaDeviceSynchronize());
    spdlog::info("IAS test complete");

    // Save image
    host::utils::try_unwrap_or_exit(image.save("minimal_sphere_output.exr"));
    
    spdlog::info("Test complete - check minimal_sphere_output.exr");

    return 0;
}