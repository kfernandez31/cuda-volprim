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
#include "thesis/device/primitive.h"

namespace thesis {

class Renderer {
   public:
    explicit Renderer(const AppConfig& config);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&& other) noexcept = default;
    Renderer& operator=(Renderer&& other) noexcept = default;

    void render();

   private:
    void initGAS();
    void uploadParams();
    void createRaygenPG();
    void createMissPG();
    void createHitgroupPG();
    void createPrimitives();
    void createPipeline();
    void saveOutput();

    AppConfig config_;

    cuda::ContextHandle cuda_ctx_;
    optix::DeviceContextHandle optix_ctx_;
    cuda::StreamHandle stream_;

    optix::TriangleGAS gas_;

    host::EnvironmentMap env_map_;
    host::Image image_;
    host::Camera camera_;
    cuda::Buffer<optix::LaunchParams> launch_params_;

    optix::ModuleHandle module_;
    optix::ProgramGroupHandle raygen_pg_;
    optix::ProgramGroupHandle miss_pg_;
    optix::ProgramGroupHandle hitgroup_pg_;
    optix::SBTHandle sbt_;
    optix::PipelineHandle pipeline_;

    cuda::Buffer<device::Primitive> primitives_; // TODO(kacper): should this be a member?
};

}  // namespace thesis
