#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <vector>
#include <iostream>
#include <fstream>

#define CUDA_CHECK(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            std::cerr << "CUDA error at " << __FILE__ << ":" << __LINE__ \
                      << " - " << cudaGetErrorString(error) << std::endl; \
            exit(1); \
        } \
    } while(0)

#define OPTIX_CHECK(call) \
    do { \
        OptixResult res = call; \
        if (res != OPTIX_SUCCESS) { \
            std::cerr << "OptiX error at " << __FILE__ << ":" << __LINE__ \
                      << " - " << optixGetErrorString(res) << std::endl; \
            exit(1); \
        } \
    } while(0)

// Minimal launch params
struct LaunchParams {
    float3* image;
    unsigned int width;
    unsigned int height;
    OptixTraversableHandle handle;
};

// Minimal CUDA kernels as strings
const char* raygen_prog = R"(
#include <optix.h>

extern "C" {
__constant__ LaunchParams params;
}

extern "C" __global__ void __raygen__rg() {
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();
    
    // Generate ray from camera at (0,0,-5) looking at origin
    float3 origin = make_float3(0.0f, 0.0f, -5.0f);
    float3 direction;
    direction.x = (idx.x / (float)dim.x - 0.5f) * 2.0f;
    direction.y = (idx.y / (float)dim.y - 0.5f) * 2.0f;
    direction.z = 1.0f;
    
    // Normalize direction
    float len = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    direction.x /= len;
    direction.y /= len;
    direction.z /= len;
    
    // Trace
    unsigned int hit = 0;
    optixTrace(params.handle,
               origin,
               direction,
               0.0f,    // tmin
               1e20f,   // tmax
               0.0f,    // rayTime
               255,     // visibilityMask
               OPTIX_RAY_FLAG_NONE,
               0,       // SBT offset
               1,       // SBT stride
               0,       // missSBTIndex
               hit);
    
    // Write result
    const unsigned int index = idx.y * params.width + idx.x;
    if (hit) {
        params.image[index] = make_float3(1.0f, 0.0f, 0.0f); // Red for hit
    } else {
        params.image[index] = make_float3(0.2f, 0.2f, 0.2f); // Gray for miss
    }
}
)";

const char* miss_prog = R"(
#include <optix.h>

extern "C" __global__ void __miss__ms() {
    optixSetPayload_0(0);
}
)";

const char* closesthit_prog = R"(
#include <optix.h>

extern "C" __global__ void __closesthit__ch() {
    optixSetPayload_0(1);
}
)";

void saveImage(const std::vector<float3>& image, int width, int height, const char* filename) {
    std::ofstream file(filename, std::ios::binary);
    file << "P6\n" << width << " " << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const float3& pixel = image[y * width + x];
            unsigned char r = static_cast<unsigned char>(pixel.x * 255.0f);
            unsigned char g = static_cast<unsigned char>(pixel.y * 255.0f);
            unsigned char b = static_cast<unsigned char>(pixel.z * 255.0f);
            file << r << g << b;
        }
    }
}

int main() {
    // Initialize CUDA
    CUDA_CHECK(cudaFree(0));
    
    // Initialize OptiX
    OPTIX_CHECK(optixInit());
    
    // Create context
    CUcontext cuCtx = 0;
    CUDA_CHECK(cudaGetCurrentContext(&cuCtx));
    
    OptixDeviceContext context = nullptr;
    OPTIX_CHECK(optixDeviceContextCreate(cuCtx, nullptr, &context));
    
    // Create module
    OptixModule module = nullptr;
    OptixPipelineCompileOptions pipeline_compile_options = {};
    
    OptixModuleCompileOptions module_compile_options = {};
    module_compile_options.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
    module_compile_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
    module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_MINIMAL;
    
    pipeline_compile_options.usesMotionBlur = false;
    pipeline_compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
    pipeline_compile_options.numPayloadValues = 1;
    pipeline_compile_options.numAttributeValues = 0;
    pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
    pipeline_compile_options.pipelineLaunchParamsVariableName = "params";
    pipeline_compile_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_SPHERE;
    
    // For minimal example, we'll skip actual PTX compilation and just create dummy module
    // In real code, you'd compile the CUDA strings to PTX first
    std::cerr << "Note: This is a structural example. PTX compilation not included." << std::endl;
    
    // Create sphere GAS
    OptixAccelBuildOptions accel_options = {};
    accel_options.buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
    accel_options.operation = OPTIX_BUILD_OPERATION_BUILD;
    
    // Sphere data
    float3 center = make_float3(0.0f, 0.0f, 0.0f);
    float radius = 1.0f;
    
    CUdeviceptr d_center, d_radius;
    CUDA_CHECK(cudaMalloc((void**)&d_center, sizeof(float3)));
    CUDA_CHECK(cudaMalloc((void**)&d_radius, sizeof(float)));
    CUDA_CHECK(cudaMemcpy((void*)d_center, &center, sizeof(float3), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy((void*)d_radius, &radius, sizeof(float), cudaMemcpyHostToDevice));
    
    // Build input
    OptixBuildInput sphere_input = {};
    sphere_input.type = OPTIX_BUILD_INPUT_TYPE_SPHERES;
    
    CUdeviceptr vertex_ptr = d_center;
    CUdeviceptr radius_ptr = d_radius;
    
    sphere_input.sphereArray.vertexBuffers = &vertex_ptr;
    sphere_input.sphereArray.numVertices = 1;
    sphere_input.sphereArray.radiusBuffers = &radius_ptr;
    
    unsigned int sphere_flags = OPTIX_GEOMETRY_FLAG_NONE;
    sphere_input.sphereArray.flags = &sphere_flags;
    sphere_input.sphereArray.numSbtRecords = 1;
    
    // Compute memory requirements
    OptixAccelBufferSizes gas_buffer_sizes;
    OPTIX_CHECK(optixAccelComputeMemoryUsage(
        context,
        &accel_options,
        &sphere_input,
        1,
        &gas_buffer_sizes));
    
    std::cout << "GAS requires temp: " << gas_buffer_sizes.tempSizeInBytes 
              << " output: " << gas_buffer_sizes.outputSizeInBytes << std::endl;
    
    // Allocate GAS buffers
    CUdeviceptr d_temp_buffer, d_output_buffer;
    CUDA_CHECK(cudaMalloc((void**)&d_temp_buffer, gas_buffer_sizes.tempSizeInBytes));
    CUDA_CHECK(cudaMalloc((void**)&d_output_buffer, gas_buffer_sizes.outputSizeInBytes));
    
    // Build GAS
    OptixTraversableHandle gas_handle;
    OPTIX_CHECK(optixAccelBuild(
        context,
        0,  // CUDA stream
        &accel_options,
        &sphere_input,
        1,
        d_temp_buffer,
        gas_buffer_sizes.tempSizeInBytes,
        d_output_buffer,
        gas_buffer_sizes.outputSizeInBytes,
        &gas_handle,
        nullptr,
        0));
    
    std::cout << "GAS built with handle: " << gas_handle << std::endl;
    
    // Setup image
    const int width = 512;
    const int height = 512;
    std::vector<float3> h_image(width * height);
    
    CUdeviceptr d_image;
    CUDA_CHECK(cudaMalloc((void**)&d_image, width * height * sizeof(float3)));
    
    // Setup launch params
    LaunchParams params;
    params.image = (float3*)d_image;
    params.width = width;
    params.height = height;
    params.handle = gas_handle;
    
    std::cout << "Minimal sphere rendering setup complete." << std::endl;
    std::cout << "Would launch " << width << "x" << height << " rays at sphere." << std::endl;
    
    // Cleanup
    CUDA_CHECK(cudaFree((void*)d_center));
    CUDA_CHECK(cudaFree((void*)d_radius));
    CUDA_CHECK(cudaFree((void*)d_temp_buffer));
    CUDA_CHECK(cudaFree((void*)d_output_buffer));
    CUDA_CHECK(cudaFree((void*)d_image));
    
    OPTIX_CHECK(optixDeviceContextDestroy(context));
    
    return 0;
}