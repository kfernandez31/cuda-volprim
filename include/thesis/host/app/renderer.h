#pragma once

#include "thesis/common/params/launch_params.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/app/config.h"
#include "thesis/host/cuda/buffer.h"
#include "thesis/host/cuda/context.h"
#include "thesis/host/cuda/stream_dag.h"
#include "thesis/host/optix/context.h"
#include "thesis/host/optix/gas.h"
#include "thesis/host/optix/ias.h"
#include "thesis/host/optix/module.h"
#include "thesis/host/optix/pipeline.h"
#include "thesis/host/optix/program_group.h"
#include "thesis/host/optix/sbt.h"
#include "thesis/host/params/camera.h"
#include "thesis/host/params/environment_map.h"
#include "thesis/host/params/image.h"

#include <cstddef>
#include <future>
#include <vector>

namespace thesis::host::app {

class Renderer {
   private:
    void initPrimsAndGAS();
    void uploadParams();
    void createPrimitives();
    void createPipeline(std::future<utils::Result<std::vector<std::byte>>> module_file_future);

    app::Config config_;

    cuda::Context cuda_ctx_;
    optix::Context optix_ctx_;
    cuda::StreamDAG streams_;

    optix::TriangleGAS<geometry::DefaultIcosphere> gas_;
    optix::IAS ias_;
    cuda::Buffer<OptixInstance> instances_;
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
    explicit Renderer(const app::Config& config);

    void render();
};

}  // namespace thesis::host::app
