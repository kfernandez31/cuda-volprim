extern "C" __global__ void __raygen__hello()
{
    const uint3 idx = optixGetLaunchIndex();
    const uint3 dim = optixGetLaunchDimensions();

    // Create a flat color: red pixel
    float4* output = reinterpret_cast<float4*>(optixLaunchParams.output_buffer);
    unsigned int offset = idx.y * dim.x + idx.x;
    output[offset] = make_float4(1.0f, 0.0f, 0.0f, 1.0f); // red
}

struct __align__(16) LaunchParams
{
    float4* output_buffer;
};

extern "C" __constant__ LaunchParams optixLaunchParams;