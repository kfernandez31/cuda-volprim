#pragma once

#include "thesis/app_config.h"
#include "thesis/cuda/context_handle.h"
#include "thesis/cuda/stream_handle.h"
#include "thesis/host/camera.h"
#include "thesis/host/environment_map.h"
#include "thesis/host/image.h"
#include "thesis/optix/gas_handle.h"
#include "thesis/optix/handle.h"
#include "thesis/optix/launch_params.h"
#include "thesis/optix/sbt_handle.h"

namespace thesis {

class Renderer {
public:
    explicit Renderer(const AppConfig& config);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&& other) = default;
    Renderer& operator=(Renderer&& other) = default;

    void render();
// private:
    void createRaygenPG();
    void createMissPG();
    void createHitgroupPG();
    void uploadParams();
    void initGAS();
    void createPipeline();
    void saveOutput();

    AppConfig config_;

    cuda::ContextHandle cuda_ctx_; // ok
    optix::DeviceContextHandle optix_ctx_; // ok
    cuda::StreamHandle stream_; // ok

    optix::TriangleGAS gas_; // ok

    host::EnvironmentMap env_map_; // ok
    host::Image image_; // ok
    host::Camera camera_; // ok
    cuda::Buffer<optix::LaunchParams> launch_params_; // ok

    optix::ModuleHandle module_; // ok
    optix::ProgramGroupHandle raygen_pg_; // ok
    optix::ProgramGroupHandle miss_pg_; // ok
    optix::ProgramGroupHandle hitgroup_pg_; // ok
    optix::SBTHandle sbt_; // ok
    optix::PipelineHandle pipeline_; // ok
};

} // namespace thesis
