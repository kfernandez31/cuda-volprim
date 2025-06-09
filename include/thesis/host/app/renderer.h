#pragma once

#include "thesis/common/params/launch_params.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/cuda/context.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/optix/gas.h"
#include "thesis/host/optix/module.h"
#include "thesis/host/optix/pipeline.h"
#include "thesis/host/optix/device_context.h"
#include "thesis/host/optix/program_group.h"
#include "thesis/host/optix/pipeline.h"
#include "thesis/host/optix/sbt.h"
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

    cuda::Context cuda_ctx_;
    optix::DeviceContext optix_ctx_;
    cuda::Stream stream_;

    optix::TriangleGAS gas_;

    host::params::EnvironmentMap env_map_;
    host::params::Image image_;
    host::params::Camera camera_;
    cuda::Buffer<device::params::Primitive> primitives_;
    cuda::Buffer<common::params::LaunchParams> launch_params_;

    optix::Module module_;
    optix::ProgramGroup raygen_pg_;
    optix::ProgramGroup miss_pg_;
    optix::ProgramGroup hitgroup_pg_;
    optix::SBT sbt_;
    optix::Pipeline pipeline_;

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
