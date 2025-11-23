#pragma once

#include "thesis/device/payloads/registry.h"
#include "thesis/host/optix/logging.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/io.h"
#include "thesis/host/utils/result.h"

#include <optix_stubs.h>

#include <cstddef>
#include <filesystem>
#include <future>
#include <spdlog/spdlog.h>
#include <string_view>
#include <utility>
#include <vector>

namespace thesis::host::optix {

class Module {
    OptixModule handle_ = nullptr;

   public:
    Module() = default;

    [[nodiscard]] static utils::Result<Module> load(OptixDeviceContext ctx,
                                                    const std::filesystem::path& filename,
                                                    const OptixPipelineCompileOptions& pco) {
        Module module;

        OptixModuleCompileOptions mco = {};
        // #ifdef DEBUG // TODO(kacper): restore
        mco.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_MODERATE;
        // #else
        // mco.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
        // #endif

        std::vector<std::byte> blob;
        TRY_ASSIGN(blob, utils::io::readFileToBytes(filename));
        spdlog::info("OptiX module loaded ({} bytes)", blob.size());

        OPTIX_CALL_LOGGED(optixModuleCreate(ctx, &mco, &pco,
                                            reinterpret_cast<const char*>(blob.data()), blob.size(),
                                            log.data(), &log_size, &module.handle_));

        return module;
    }

    // Async version: takes future from utils::io::readFileToBytesAsync() and creates module
    [[nodiscard]] static utils::Result<Module> loadAsync(
        OptixDeviceContext ctx, std::future<utils::Result<std::vector<std::byte>>>& file_future,
        const OptixPipelineCompileOptions& pco) {
        Module module;

        OptixModuleCompileOptions mco = {};
        // #ifdef DEBUG // TODO(kacper): restore
        mco.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_MODERATE;
        // #else
        // mco.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
        // #endif

        std::vector<std::byte> blob;
        TRY_ASSIGN(blob, file_future.get());
        spdlog::info("OptiX module loaded ({} bytes)", blob.size());

        OPTIX_CALL_LOGGED(optixModuleCreate(ctx, &mco, &pco,
                                            reinterpret_cast<const char*>(blob.data()), blob.size(),
                                            log.data(), &log_size, &module.handle_));

        return module;
    }

    ~Module() { reset(); }

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    Module(Module&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Module& operator=(Module&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] OptixModule get() const noexcept { return handle_; }

   private:
    void reset() {
        if (handle_) {
            OPTIX_CHECK(optixModuleDestroy(handle_));
        }
    }
};

}  // namespace thesis::host::optix