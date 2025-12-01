#include "thesis/host/app/renderer.h"

#include "thesis/pch.h"

#include "thesis/common/geometry/intersection.h"
#include "thesis/common/utils/math.h"
#include "thesis/device/utils/vector.h"
#include "thesis/host/optix/logging.h"
#include "thesis/host/params/primitive.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/math.h"
#include "thesis/host/utils/result.h"

#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>

namespace thesis::host::app {

// Batch size for progressive rendering (tuned for RTX 2080 with 8GB VRAM)
// For 1080p: 6.5GB / (1920×1080×16) ≈ 200 samples/batch
// For 4K: 6.5GB / (3840×2160×16) ≈ 50 samples/batch
// Using conservative value that works well for both
constexpr size_t BATCH_SIZE = 64;

Renderer::Renderer(const app::Config& config, std::vector<params::Primitive>&& primitives)
    // clang-format off
    : config_(config),
      num_primitives_(primitives.size()),
      cuda_ctx_(),
      optix_ctx_(cuda_ctx_.get()),
      streams_(),
      gas_(cuda_ctx_.get(), streams_[cuda::StreamKind::GAS]),
      ias_(cuda_ctx_.get(), streams_[cuda::StreamKind::IAS]),
      instances_(num_primitives_, cuda_ctx_.get(), streams_[cuda::StreamKind::IAS], cuda::AllocType::OnBoth),
      sbt_(cuda_ctx_.get(), streams_[cuda::StreamKind::SBT]),
      env_map_(utils::io::async::loadHDR(config_.env_map_path_), cuda_ctx_.get(), streams_[cuda::StreamKind::EnvMap]),
      image_(config_.image_width_, config_.image_height_, config_.num_samples_per_pixel_, BATCH_SIZE, cuda_ctx_.get(), streams_[cuda::StreamKind::Image], streams_[cuda::StreamKind::Main]),
      camera_(host::params::Camera::getDefaultCamera(config.image_width_, config.image_height_)),
      primitives_(num_primitives_, cuda_ctx_.get(), streams_[cuda::StreamKind::Prims], cuda::AllocType::OnBoth),
      camera_active_prims_(0, cuda_ctx_.get(), streams_[cuda::StreamKind::Main], cuda::AllocType::OnBoth),
      launch_params_(1, cuda_ctx_.get(), streams_[cuda::StreamKind::Main], cuda::AllocType::OnBoth) {
    // clang-format on

    auto module_file_future = utils::io::async::readFileToBytes(config_.module_blob_path_);

    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::EnvMap);
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Image);

    initPrimsAndGAS(std::move(primitives));
    createPipeline(std::move(module_file_future));
    initStaticParams();  // Initialize static parameters once (camera-inside, etc.)
}

Renderer::~Renderer() {
    if (builtin_is_module_) {
        optixModuleDestroy(builtin_is_module_);
    }
}

void Renderer::initPrimsAndGAS(std::vector<params::Primitive>&& primitives) {
    /* ── 1. Build GAS with one unit sphere ───────────────────────── */
    gas_.build(cuda_ctx_.get(), optix_ctx_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::GAS);

    /* ── 2. Fill primitives_[] and OptixInstance[] ────────────────── */
    for (size_t i = 0; i < num_primitives_; ++i) {
        // Convert host primitive to device primitive
        const auto& prim = primitives[i];
        primitives_[i] = prim.device_primitive();

        OptixInstance inst{};

        const auto transform = prim.localToWorld();
        std::memcpy(inst.transform, transform.ptr(), 12 * sizeof(float));

        inst.traversableHandle = gas_.get();
        inst.instanceId = static_cast<uint>(i);
        inst.sbtOffset = 0;
        inst.visibilityMask = 0xFF;
        inst.flags = OPTIX_INSTANCE_FLAG_NONE;
        instances_[i] = inst;
    }

    primitives_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::Prims);

    instances_.upload();
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::IAS);
    streams_.addDependency(cuda::StreamKind::IAS, cuda::StreamKind::GAS);

    /* ── 3. Build IAS over instances ──────────────────────────────── */
    OptixBuildInput bi{};
    bi.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    bi.instanceArray.instances = instances_.cu_device_ptr();
    bi.instanceArray.numInstances = static_cast<unsigned int>(num_primitives_);

    ias_.build(bi, cuda_ctx_.get(), optix_ctx_.get());
}

void Renderer::initStaticParams() {
    // Compute which primitives contain camera (CPU side, done once at init)
    std::vector<uint> camera_active;
    const auto camera_pos = camera_.lookfrom();  // Camera position

    for (size_t i = 0; i < num_primitives_; ++i) {
        // Get device primitive struct from buffer
        const auto prim_device = primitives_[i];

        // Check if camera is inside this primitive using shared intersection code
        if (common::geometry::point_inside_ellipsoid(camera_pos, prim_device)) {
            camera_active.push_back(static_cast<uint>(i));
        }
    }

    // Allocate and upload camera active prims (only if non-empty)
    if (!camera_active.empty()) {
        camera_active_prims_ =
            cuda::AsyncBuffer<uint>(camera_active.size(), cuda_ctx_.get(),
                                    streams_[cuda::StreamKind::Main], cuda::AllocType::OnBoth);
        std::memcpy(camera_active_prims_.host(), camera_active.data(),
                    camera_active.size() * sizeof(uint));
        camera_active_prims_.upload();
    } else {
        camera_active_prims_ = cuda::AsyncBuffer<uint>(
            0, cuda_ctx_.get(), streams_[cuda::StreamKind::Main], cuda::AllocType::OnBoth);
    }

    // Initialize static launch parameters (never change during rendering)
    auto& par = launch_params_[0];
    par.seed_ = config_.seed_;
    par.debug_ = config_.debug_;
    par.ias_handle_ = ias_.get();
    par.camera_ = camera_.device_camera();
    par.env_map_ = env_map_.device_env_map();
    par.primitives_ = device::utils::DynamicVector<device::params::Primitive>(primitives_.device(),
                                                                              primitives_.size());
    par.camera_active_prims_ =
        device::utils::DynamicVector<uint>(camera_active_prims_.device(), camera_active.size());

    // Image will be set by updateDynamicParams() before each batch
    par.image_ = image_.device_image();

    launch_params_.upload();
}

void Renderer::updateDynamicParams() {
    // Update only dynamic parameters that change between batches
    // Currently only image_ changes (batch_offset, batch_size)
    launch_params_[0].image_ = image_.device_image();
    launch_params_.upload();
}

void Renderer::createPipeline(
    std::future<utils::Result<std::vector<std::byte>>> module_file_future) {
    OptixPipelineCompileOptions pco{};
    pco.pipelineLaunchParamsVariableName = config_.launch_params_variable_name_.c_str();
    pco.numPayloadValues = device::payloads::MAX_PAYLOADS_IN_USE;
    pco.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING |
                                OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
    pco.usesPrimitiveTypeFlags =
        static_cast<uint>(OPTIX_PRIMITIVE_TYPE_FLAGS_SPHERE);  // Using built-in spheres
    pco.numAttributeValues = 0;

    // Complete async module load
    module_ = utils::try_unwrap_or_exit<optix::Module>(
        optix::Module::loadAsync(optix_ctx_.get(), module_file_future, pco));

    // Get built-in sphere intersection module
    OptixBuiltinISOptions builtin_is_options{};
    builtin_is_options.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_SPHERE;
    builtin_is_options.usesMotionBlur = false;
    builtin_is_options.buildFlags = optix::GAS_BUILD_FLAGS;

    OptixModuleCompileOptions builtin_mco{};
    builtin_mco.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;

#ifdef DEBUG
    builtin_mco.optLevel = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
    builtin_mco.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_MINIMAL;
#else
    builtin_mco.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_3;
    builtin_mco.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
#endif

    OPTIX_CHECK(optixBuiltinISModuleGet(optix_ctx_.get(), &builtin_mco, &pco, &builtin_is_options,
                                        &builtin_is_module_));

    // raygen
    raygen_pg_ = optix::ProgramGroup::createRaygen(optix_ctx_.get(), module_.get(),
                                                   config_.raygen_function_name_.c_str());

    // miss
    miss_pg_ = optix::ProgramGroup::createMiss(optix_ctx_.get(), module_.get(),
                                               config_.miss_function_name_.c_str());

    // Create hitgroup with anyhit + built-in sphere intersection
    // Closesthit disabled via OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT in trace.cuh
    hitgroup_pg_ = optix::ProgramGroup::createHitgroup(
        optix_ctx_.get(), module_.get(), config_.anyhit_function_name_.c_str(),
        builtin_is_module_  // Built-in sphere intersection module
    );

    // sbt
    sbt_.build(raygen_pg_.get(), miss_pg_.get(), hitgroup_pg_.get());
    streams_.addDependency(cuda::StreamKind::Main, cuda::StreamKind::SBT);
    spdlog::debug("SBT created");

    OptixPipelineLinkOptions plo{};
    plo.maxTraceDepth = 1;

    std::array<OptixProgramGroup, 3> pgs = {raygen_pg_.get(), miss_pg_.get(), hitgroup_pg_.get()};

    pipeline_ = optix::Pipeline(optix_ctx_.get(), pco, plo, pgs.data(), pgs.size());
    spdlog::info("OptiX pipeline built");
}

void Renderer::render() {
    const size_t total_spp = image_.num_samples_per_pixel();
    const size_t batch_size = image_.batch_size();
    const size_t num_batches = common::math::ceil_div(total_spp, batch_size);

    spdlog::info(
        "Launching OptiX pipeline - will render {} Gaussians in {} batches ({} spp total)...",
        num_primitives_, num_batches, total_spp);

    // Render in batches
    for (size_t batch = 0; batch < num_batches; ++batch) {
        const size_t batch_offset = batch * batch_size;
        const size_t samples_in_batch = common::math::min(batch_size, total_spp - batch_offset);

        // Update batch parameters
        image_.set_batch_params(batch_offset, samples_in_batch);

        // Update dynamic launch params (image with new batch_offset/batch_size)
        updateDynamicParams();

        spdlog::info("Rendering batch {}/{}: samples [{}, {})", batch + 1, num_batches,
                     batch_offset, batch_offset + samples_in_batch);

        // Launch with 2D grid (width, height) - batch handled internally by raygen
        pipeline_.launch(streams_[cuda::StreamKind::Main]->get(), launch_params_.cu_device_ptr(),
                         sizeof(common::params::LaunchParams), sbt_.get(), image_.width(),
                         image_.height(),
                         1);  // Z=1, batching handled in raygen kernel

        // Synchronize to ensure this batch completes before next batch reads accumulator
        streams_[cuda::StreamKind::Main]->synchronize();
    }

    spdlog::info("All batches complete, saving image...");

    // Save asynchronously and wait for completion
    auto save_future = image_.save(config_.output_path_);
    utils::try_unwrap_or_exit(save_future.get());
    spdlog::info("Rendering complete");
}

}  // namespace thesis::host::app
