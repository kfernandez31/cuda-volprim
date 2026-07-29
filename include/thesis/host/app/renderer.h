#pragma once

#include "thesis/common/params/launch_params.h"
#include "thesis/device/params/primitive.h"
#include "thesis/host/app/config.h"
#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/cuda/context.h"
#include "thesis/host/cuda/stream_dag.h"
#include "thesis/host/optix/context.h"
#include "thesis/host/optix/denoiser.h"
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
#include <optional>
#include <vector>

namespace thesis::host::app {

class Renderer {
   private:
    void initPrimsAndGAS(std::vector<device::params::Primitive>&& primitives);
    void initStaticParams();
    void updateDynamicParams();
    void createPipeline(std::future<utils::Result<std::vector<std::byte>>> module_file_future);

    app::Config config_;
    size_t num_primitives_;

    cuda::Context cuda_ctx_;
    optix::Context optix_ctx_;
    cuda::StreamDAG streams_;

#ifdef THESIS_ICOSPHERE
    optix::IcosphereGAS gas_;  // tessellated icosphere (Ch 6 G8 A/B) — same interface as SphereGAS
#else
    optix::SphereGAS gas_;     // analytic built-in sphere (production default)
#endif
    optix::IAS ias_;
    host::params::EnvironmentMap env_map_;
    host::params::Image image_;
    host::params::Camera camera_;
    cuda::AsyncBuffer<device::params::Primitive> primitives_;
    // Device-only buffer for the kernel; the host-side mirror lives next to it
    // as a plain struct to avoid burning a 4 KB pinned page on a single
    // ~few-hundred-byte object. Uploads are stream-ordered cudaMemcpyAsync
    // from pageable memory (CUDA stages internally for sub-MB transfers).
    cuda::AsyncBuffer<common::params::LaunchParams> launch_params_;
    common::params::LaunchParams launch_params_host_{};

    // Single-element device counter for cap-overflow events (active-prims / hit-buffer
    // drops). Read back after the render to warn about silently-biased dense regions.
    cuda::AsyncBuffer<unsigned long long> overflow_counter_;

    // Two-slot maxima buffer for --measure-caps: [0]=max hits/ray, [1]=max point-overlap.
    // Allocated only when config_.measure_caps_ is true; device side gated on render_.measure_caps_.
    cuda::AsyncBuffer<uint32_t> measure_buf_;

    // Precomputed bounce-0 origin-inside set for shared-origin (perspective) cameras;
    // filled once per render by device/kernels/camera_active_set.cu. Unallocated (and the
    // launch-param pointer null) for orthographic cameras and under --measure-caps.
    cuda::AsyncBuffer<uint32_t> camera_active_set_;

    optix::Module module_;
    optix::Module builtin_is_module_;
    optix::ProgramGroup raygen_pg_;
    optix::ProgramGroup miss_pg_;
    optix::ProgramGroup hitgroup_pg_;
    optix::SBT sbt_;
    optix::Pipeline pipeline_;
    std::optional<optix::Denoiser> denoiser_;

   public:
    explicit Renderer(const app::Config& config,
                      std::vector<device::params::Primitive>&& primitives,
                      std::optional<params::Camera> camera = std::nullopt);

    void render();
};

}  // namespace thesis::host::app
