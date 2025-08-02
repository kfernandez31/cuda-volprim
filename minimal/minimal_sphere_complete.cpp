// Minimal OptiX Sphere Rendering Example
// This demonstrates the absolute minimum needed to render a sphere with OptiX

#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <iostream>
#include <vector>

#define CHECK_CUDA(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            std::cerr << "CUDA error: " << cudaGetErrorString(error) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            exit(1); \
        } \
    } while(0)

#define CHECK_OPTIX(call) \
    do { \
        OptixResult res = call; \
        if (res != OPTIX_SUCCESS) { \
            std::cerr << "OptiX error: " << optixGetErrorString(res) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            exit(1); \
        } \
    } while(0)

int main() {
    // 1. Initialize CUDA and OptiX
    CHECK_CUDA(cudaFree(0));
    CHECK_OPTIX(optixInit());
    
    CUcontext cuCtx = 0;
    CHECK_CUDA(cudaGetCurrentContext(&cuCtx));
    
    OptixDeviceContext context = nullptr;
    CHECK_OPTIX(optixDeviceContextCreate(cuCtx, nullptr, &context));
    
    // 2. Create sphere geometry
    float3 sphere_center = make_float3(0.0f, 0.0f, 0.0f);
    float sphere_radius = 1.0f;
    
    // Allocate and upload sphere data
    CUdeviceptr d_centers, d_radii;
    CHECK_CUDA(cudaMalloc((void**)&d_centers, sizeof(float3)));
    CHECK_CUDA(cudaMalloc((void**)&d_radii, sizeof(float)));
    CHECK_CUDA(cudaMemcpy((void*)d_centers, &sphere_center, sizeof(float3), cudaMemcpyHostToDevice));
    CHECK_CUDA(cudaMemcpy((void*)d_radii, &sphere_radius, sizeof(float), cudaMemcpyHostToDevice));
    
    // 3. Build sphere GAS
    OptixBuildInput sphere_input = {};
    sphere_input.type = OPTIX_BUILD_INPUT_TYPE_SPHERES;
    
    // CRITICAL: These pointers must remain valid during build!
    CUdeviceptr vertex_buffer = d_centers;
    CUdeviceptr radius_buffer = d_radii;
    
    sphere_input.sphereArray.vertexBuffers = &vertex_buffer;
    sphere_input.sphereArray.numVertices = 1;
    sphere_input.sphereArray.radiusBuffers = &radius_buffer;
    sphere_input.sphereArray.singleRadius = 0;  // Use radius buffer
    
    unsigned int sphere_flags = OPTIX_GEOMETRY_FLAG_NONE;
    sphere_input.sphereArray.flags = &sphere_flags;
    sphere_input.sphereArray.numSbtRecords = 1;
    sphere_input.sphereArray.primitiveIndexOffset = 0;
    
    // Build options
    OptixAccelBuildOptions accel_options = {};
    accel_options.buildFlags = OPTIX_BUILD_FLAG_NONE;  // No compaction for debugging
    accel_options.operation = OPTIX_BUILD_OPERATION_BUILD;
    
    // Get buffer sizes
    OptixAccelBufferSizes gas_buffer_sizes;
    CHECK_OPTIX(optixAccelComputeMemoryUsage(
        context,
        &accel_options,
        &sphere_input,
        1,  // num build inputs
        &gas_buffer_sizes
    ));
    
    std::cout << "GAS buffer sizes - temp: " << gas_buffer_sizes.tempSizeInBytes 
              << ", output: " << gas_buffer_sizes.outputSizeInBytes << std::endl;
    
    // Allocate buffers
    CUdeviceptr d_temp_buffer, d_output_buffer;
    CHECK_CUDA(cudaMalloc((void**)&d_temp_buffer, gas_buffer_sizes.tempSizeInBytes));
    CHECK_CUDA(cudaMalloc((void**)&d_output_buffer, gas_buffer_sizes.outputSizeInBytes));
    
    // Build the GAS
    OptixTraversableHandle gas_handle = 0;
    CHECK_OPTIX(optixAccelBuild(
        context,
        0,  // CUDA stream
        &accel_options,
        &sphere_input,
        1,  // num build inputs
        d_temp_buffer,
        gas_buffer_sizes.tempSizeInBytes,
        d_output_buffer,
        gas_buffer_sizes.outputSizeInBytes,
        &gas_handle,
        nullptr,  // emitted properties
        0         // num emitted properties
    ));
    
    CHECK_CUDA(cudaDeviceSynchronize());
    
    std::cout << "Successfully built sphere GAS with handle: 0x" << std::hex << gas_handle << std::dec << std::endl;
    
    // 4. Key debugging checks
    if (gas_handle == 0) {
        std::cerr << "ERROR: GAS handle is 0!" << std::endl;
        return 1;
    }
    
    if (gas_buffer_sizes.outputSizeInBytes < 1000) {
        std::cerr << "WARNING: GAS output size seems too small: " << gas_buffer_sizes.outputSizeInBytes << std::endl;
    }
    
    // 5. Create a simple IAS (instance acceleration structure)
    OptixInstance instance = {};
    
    // Identity transform
    instance.transform[0] = 1.0f;
    instance.transform[4] = 1.0f;
    instance.transform[8] = 1.0f;
    
    instance.instanceId = 0;
    instance.sbtOffset = 0;
    instance.visibilityMask = 255;
    instance.flags = OPTIX_INSTANCE_FLAG_NONE;
    instance.traversableHandle = gas_handle;
    
    CUdeviceptr d_instance;
    CHECK_CUDA(cudaMalloc((void**)&d_instance, sizeof(OptixInstance)));
    CHECK_CUDA(cudaMemcpy((void*)d_instance, &instance, sizeof(OptixInstance), cudaMemcpyHostToDevice));
    
    // Build IAS
    OptixBuildInput instance_input = {};
    instance_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    instance_input.instanceArray.instances = d_instance;
    instance_input.instanceArray.numInstances = 1;
    
    OptixAccelBufferSizes ias_buffer_sizes;
    CHECK_OPTIX(optixAccelComputeMemoryUsage(context, &accel_options, &instance_input, 1, &ias_buffer_sizes));
    
    CUdeviceptr d_ias_temp, d_ias_output;
    CHECK_CUDA(cudaMalloc((void**)&d_ias_temp, ias_buffer_sizes.tempSizeInBytes));
    CHECK_CUDA(cudaMalloc((void**)&d_ias_output, ias_buffer_sizes.outputSizeInBytes));
    
    OptixTraversableHandle ias_handle = 0;
    CHECK_OPTIX(optixAccelBuild(
        context, 0, &accel_options, &instance_input, 1,
        d_ias_temp, ias_buffer_sizes.tempSizeInBytes,
        d_ias_output, ias_buffer_sizes.outputSizeInBytes,
        &ias_handle, nullptr, 0
    ));
    
    std::cout << "Successfully built IAS with handle: 0x" << std::hex << ias_handle << std::dec << std::endl;
    
    // Final diagnostic
    std::cout << "\nDiagnostics:" << std::endl;
    std::cout << "- Sphere center: (" << sphere_center.x << ", " << sphere_center.y << ", " << sphere_center.z << ")" << std::endl;
    std::cout << "- Sphere radius: " << sphere_radius << std::endl;
    std::cout << "- GAS handle: 0x" << std::hex << gas_handle << std::dec << std::endl;
    std::cout << "- IAS handle: 0x" << std::hex << ias_handle << std::dec << std::endl;
    std::cout << "- Use IAS handle (0x" << std::hex << ias_handle << std::dec << ") for ray tracing" << std::endl;
    
    // Cleanup
    CHECK_CUDA(cudaFree((void*)d_centers));
    CHECK_CUDA(cudaFree((void*)d_radii));
    CHECK_CUDA(cudaFree((void*)d_temp_buffer));
    CHECK_CUDA(cudaFree((void*)d_output_buffer));
    CHECK_CUDA(cudaFree((void*)d_instance));
    CHECK_CUDA(cudaFree((void*)d_ias_temp));
    CHECK_CUDA(cudaFree((void*)d_ias_output));
    
    CHECK_OPTIX(optixDeviceContextDestroy(context));
    
    return 0;
}