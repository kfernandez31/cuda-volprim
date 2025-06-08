#pragma once

#include "thesis/common/params/launch_params.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/cuda/context_handle.h"
#include "thesis/host/cuda/stream_handle.h"
#include "thesis/host/optix/gas_handle.h"
#include "thesis/host/optix/handle.h"
#include "thesis/host/optix/sbt_handle.h"
#include "thesis/host/params/camera.h"
#include "thesis/host/params/environment_map.h"
#include "thesis/host/params/image.h"
#include "thesis/host/app/config.h"

namespace thesis::host::app {

class Renderer {
   private:
    void initGAS();
    void initPrimitives();
    void uploadParams();
    void createRaygenPG();
    void createMissPG();
    void createHitgroupPG();
    void createPrimitives();
    void createPipeline();

    app::Config config_;

    cuda::ContextHandle cuda_ctx_;
    optix::DeviceContextHandle optix_ctx_;
    cuda::StreamHandle stream_;

    optix::TriangleGAS gas_;

    host::EnvironmentMap env_map_;
    host::Image image_;
    host::Camera camera_;
    cuda::Buffer<device::params::Primitive> primitives_;
    cuda::Buffer<common::params::LaunchParams> launch_params_;

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

    explicit Renderer(const app::Config& config);

    void render();
};

}  // namespace thesis::host::app
