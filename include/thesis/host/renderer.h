#pragma once

#include "thesis/host/utils/app_config.h"
#include "thesis/host/cuda/context_handle.h"
#include "thesis/host/cuda/stream_handle.h"
#include "thesis/host/optix/gas_handle.h"
#include "thesis/host/optix/handle.h"
#include "thesis/host/optix/launch_params.h"
#include "thesis/host/optix/sbt_handle.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/params/camera.h"
#include "thesis/host/params/environment_map.h"
#include "thesis/host/params/image.h"

namespace thesis {

class Renderer {
   private:
    void initGAS();
    void uploadParams();
    void createRaygenPG();
    void createMissPG();
    void createHitgroupPG();
    void createPrimitives();
    void createPipeline();

    AppConfig config_;

    cuda::ContextHandle cuda_ctx_;
    optix::DeviceContextHandle optix_ctx_;
    cuda::StreamHandle stream_;

    optix::TriangleGAS gas_;

    host::EnvironmentMap env_map_;
    host::Image image_;
    host::Camera camera_;
    cuda::Buffer<device::Primitive> primitives_;
    cuda::Buffer<optix::LaunchParams> launch_params_;

    optix::ModuleHandle module_;
    optix::ProgramGroupHandle raygen_pg_;
    optix::ProgramGroupHandle miss_pg_;
    optix::ProgramGroupHandle hitgroup_pg_;
    optix::SBTHandle sbt_;
    optix::PipelineHandle pipeline_;
   public:
    Renderer() = delete;

    Renderer(Renderer&&) noexcept = default;
    Renderer& operator=(Renderer&&) noexcept = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    explicit Renderer(const AppConfig& config);

    void render();
};

}  // namespace thesis
