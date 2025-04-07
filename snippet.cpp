#define OPTIX_USE_HOST_API
#include <optix_stubs.h>
#include <optix_function_table.h>

/*
cl.exe /std:c++20 /I?"C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.0.0\include" /I"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\include" src/snippet.cpp /link /LIBPATH:"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\lib\x64" cuda.lib
*/

OptixFunctionTable g_optixFunctionTable_105 = {}; // 👈 required for linking

int main() {
    optixInit(); // THIS IS REQUIRED!

    OptixDeviceContext ctx = nullptr;
    OptixModule module;
    OptixPipelineCompileOptions options = {};
    OptixModuleCompileOptions modOptions = {};

    optixModuleCreate(ctx, &modOptions, &options, "dummy.ptx", 0, nullptr, nullptr, &module);

    return 0;
}