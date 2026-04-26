#include "thesis/host/utils/io.h"

#include "thesis/pch.h"

#include "thesis/host/cuda/async_buffer.h"

#include <vector_types.h>

#include <algorithm>
#include <array>
#include <execution>
#include <fstream>
#include <ios>
#include <numeric>
#include <ranges>
#include <stb/stb_image.h>
#include <string>
#include <tinyexr/tinyexr.h>

#ifdef _MSC_VER
#include <cstdlib>
#endif  // _MSC_VER

namespace {

using namespace thesis::host::utils;

constexpr size_t NUM_CHANNELS = 3;
constexpr size_t EXR_NAME_MAX_LEN = 255;

inline void safeStrncpy(char* dest, const char* src, size_t dest_size) noexcept {
#ifdef _MSC_VER
    strncpy_s(dest, dest_size, src, _TRUNCATE);
#else
    if (dest_size == 0) {
        return;
    }
    std::strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
#endif  // _MSC_VER
}

// Internal helper for reading files
Result<std::vector<std::byte>> readFile(const std::filesystem::path& filename) {
    try {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file) {
            return make_error("Failed to open file: {}", filename.string());
        }

        const auto file_size = file.tellg();
        if (file_size <= 0) {
            return make_error("File is empty or error reading file size: {}", filename.string());
        }

        std::vector<std::byte> buffer(static_cast<size_t>(file_size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);

        if (!file) {
            return make_error("Error while reading file: {}", filename.string());
        }

        return buffer;
    } catch (const std::exception& e) {
        return make_error("Exception in readFile: {}", e.what());
    }
}

// Internal helper for saving EXR
Result<> saveExrImage(std::span<const float4> framebuffer, size_t width, size_t height,
                      const std::filesystem::path& filename, bool flip_vertical) noexcept {
    try {
        constexpr std::array<const char*, NUM_CHANNELS> channel_names = {"B", "G", "R"};

        std::array<std::vector<float>, NUM_CHANNELS> channels;
        for (auto& chan : channels) {
            chan.resize(width * height);
        }

        // Parallelize channel deinterleaving over rows
        auto rows = std::views::iota(size_t{0}, height);
        std::for_each(std::execution::par, rows.begin(), rows.end(), [&](size_t y) {
            const auto row_in = y * width;
            const auto row_out = (flip_vertical ? height - 1 - y : y) * width;

            for (size_t x = 0; x < width; ++x) {
                const auto& c = framebuffer[row_in + x];
                channels[0][row_out + x] = c.x;  // R
                channels[1][row_out + x] = c.y;  // G
                channels[2][row_out + x] = c.z;  // B (W component ignored)
            }
        });

        EXRImage image;
        InitEXRImage(&image);

        std::array<float*, NUM_CHANNELS> channel_ptrs = {
            channels[2].data(),  // B
            channels[1].data(),  // G
            channels[0].data(),  // R
        };

        image.num_channels = static_cast<int>(NUM_CHANNELS);
        image.images = reinterpret_cast<unsigned char**>(channel_ptrs.data());
        image.width = static_cast<int>(width);
        image.height = static_cast<int>(height);

        EXRHeader header;
        InitEXRHeader(&header);

        std::array<EXRChannelInfo, NUM_CHANNELS> channel_info = {};
        for (size_t i = 0; i < NUM_CHANNELS; ++i) {
            safeStrncpy(channel_info[i].name, channel_names[i], EXR_NAME_MAX_LEN);
        }

        header.channels = channel_info.data();
        header.num_channels = NUM_CHANNELS;

        std::array<int, NUM_CHANNELS> pixel_types;
        pixel_types.fill(TINYEXR_PIXELTYPE_FLOAT);
        header.pixel_types = pixel_types.data();
        header.requested_pixel_types = pixel_types.data();

        const char* err = nullptr;
        if (SaveEXRImageToFile(&image, &header, filename.string().c_str(), &err) !=
            TINYEXR_SUCCESS) {
            std::string err_msg(err);
            FreeEXRErrorMessage(err);
            return make_error("EXR save failed: {}", err_msg);
        }

        return {};
    } catch (const std::exception& e) {
        return make_error("Exception in saveExrImage: {}", e.what());
    }
}

// Internal helper for loading HDR
Result<thesis::host::utils::io::HDRImageData> loadHDRImage(const std::filesystem::path& filename) {
    spdlog::info("Loading environment map from '{}'", filename.string());

    stbi_set_flip_vertically_on_load(true);

    int w, h, c;
    // Force RGBA format (4 channels) for CUDA texture compatibility
    auto* raw = stbi_loadf(filename.string().c_str(), &w, &h, &c, 4);
    if (!raw) {
        return make_error("Failed to load HDR image: {}", filename.string());
    }

    const size_t width = static_cast<size_t>(w);
    const size_t height = static_cast<size_t>(h);
    const size_t channels = 4;  // Always RGBA now

    // Allocate pinned memory for true async CUDA transfers
    const size_t total_floats = width * height * channels;
    const size_t total_bytes = total_floats * sizeof(float);

    float* pinned_mem = nullptr;
    cudaError_t err = cudaHostAlloc(&pinned_mem, total_bytes, cudaHostAllocDefault);

    if (err != cudaSuccess || !pinned_mem) {
        stbi_image_free(raw);
        return make_error("Failed to allocate pinned memory for HDR image: {}",
                          cudaGetErrorString(err));
    }

    // Copy from pageable to pinned memory (happens on background thread)
    std::memcpy(pinned_mem, raw, total_bytes);

    // Free stb_image memory
    stbi_image_free(raw);

    using namespace thesis::host::utils::io;
    return HDRImageData{.data = HDRImagePtr(pinned_mem, CudaPinnedDeleter{}),
                        .width = width,
                        .height = height,
                        .channels = channels};
}

// Internal helper for loading primitives
Result<std::vector<thesis::device::params::Primitive>> loadPrimitivesFromPLY(
    const std::filesystem::path& filename, float sigma_multiplier, float3 albedo_override) {
    try {
        happly::PLYData ply(filename.string());

        size_t N = 0;
        auto& vtx = ply.getElement("vertex");

        auto get_prop = [&](const std::string& name) -> std::vector<float> {
            try {
                auto prop = vtx.getProperty<float>(name);
                if (N != 0 && prop.size() != N) [[unlikely]] {
                    throw std::runtime_error(std::string("Expected size ") + std::to_string(N) +
                                             ", got " + std::to_string(prop.size()));
                }
                return prop;
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("Property \"") + name + "\": " + e.what());
            }
        };

        auto sigma_t = get_prop("sigma_t_0");
        N = sigma_t.size();

        auto p_x = get_prop("x");
        auto p_y = get_prop("y");
        auto p_z = get_prop("z");

        auto rot_0 = get_prop("rot_0");
        auto rot_1 = get_prop("rot_1");
        auto rot_2 = get_prop("rot_2");
        auto rot_3 = get_prop("rot_3");

        auto scale_0 = get_prop("scale_0");
        auto scale_1 = get_prop("scale_1");
        auto scale_2 = get_prop("scale_2");

        auto alb_0 = get_prop("albedo_0");
        auto alb_1 = get_prop("albedo_1");
        auto alb_2 = get_prop("albedo_2");

        using namespace thesis::host;
        using namespace thesis::common::geometry;
        using Primitive = thesis::device::params::Primitive;

        // Phase 1: Parallel construction (expf, quaternion math, Primitive precomputation)
        std::vector<Primitive> result(N, Primitive{});

        auto indices = std::views::iota(size_t{0}, N);
        std::for_each(std::execution::par, indices.begin(), indices.end(), [&](size_t i) {
            auto center = make_float3(p_x[i], p_y[i], p_z[i]);
            auto quat = UnitQuaternion::from(rot_0[i], rot_1[i], rot_2[i], rot_3[i]);
            auto scale = make_float3(expf(scale_0[i]), expf(scale_1[i]), expf(scale_2[i]));
            auto albedo = (albedo_override.x >= 0.0f && albedo_override.y >= 0.0f &&
                           albedo_override.z >= 0.0f)
                              ? albedo_override
                              : make_float3(alb_0[i], alb_1[i], alb_2[i]);
            // Convert from Jorge's peak-extinction convention to DSYG unnormalized convention:
            // Jorge:  σ(x) = sigmat * exp(-0.5 * |x_local|²)
            // DSYG:   σ(x) = optical_thickness * (2π)^{-3/2} * ∏(1/s) * exp(-0.5 * |x_local|²)
            // Match peaks: optical_thickness = sigmat * (2π)^{3/2} * ∏(s)
            auto sigmat = expf(sigma_t[i]) * sigma_multiplier;
            auto optical_thickness =
                sigmat * thesis::common::math::TWO_PI_POW_3_2_F * scale.x * scale.y * scale.z;

            result[i] =
                Primitive::from_forward_quat(center, quat, scale, albedo, optical_thickness);
        });

        // Phase 2: Sequential validation (early exit on error)
        for (size_t i = 0; i < N; ++i) {
            const auto scale = result[i].scale();
            const auto center = result[i].center();
            const auto optical_thickness = result[i].optical_thickness_;

            if (scale.x <= 0.0f || scale.y <= 0.0f || scale.z <= 0.0f) {
                return make_error(
                    "Primitive {}: Invalid scale ({}, {}, {}) - all components must be > 0", i,
                    scale.x, scale.y, scale.z);
            }
            if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z)) {
                return make_error("Primitive {}: NaN/Inf in scale ({}, {}, {})", i, scale.x,
                                  scale.y, scale.z);
            }
            if (optical_thickness <= 0.0f) {
                return make_error("Primitive {}: Invalid optical_thickness {} - must be > 0", i,
                                  optical_thickness);
            }
            if (!std::isfinite(optical_thickness)) {
                return make_error("Primitive {}: NaN/Inf in optical_thickness {}", i,
                                  optical_thickness);
            }
            if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) {
                return make_error("Primitive {}: NaN/Inf in center ({}, {}, {})", i, center.x,
                                  center.y, center.z);
            }

            // Non-critical: clamp and warn
            auto& albedo = result[i].albedo_;
            auto clamp_albedo = [&](float& val, const char* component) {
                if (!std::isfinite(val)) {
                    spdlog::warn("Primitive {}: NaN/Inf in albedo.{}, setting to 0", i, component);
                    val = 0.0f;
                } else if (val < 0.0f) {
                    spdlog::warn("Primitive {}: Negative albedo.{} = {}, clamping to 0", i,
                                 component, val);
                    val = 0.0f;
                } else if (val > 1.0f) {
                    spdlog::warn("Primitive {}: albedo.{} = {} > 1.0, clamping to 1.0", i,
                                 component, val);
                    val = 1.0f;
                }
            };
            clamp_albedo(albedo.x, "r");
            clamp_albedo(albedo.y, "g");
            clamp_albedo(albedo.z, "b");
        }

        return result;

    } catch (const std::exception& e) {
        return make_error("Failed to load primitives from {}: {}", filename.string(), e.what());
    }
}

}  // anonymous namespace

namespace thesis::host::utils::io {

void CudaPinnedDeleter::operator()(float* ptr) const noexcept {
    if (ptr) {
        CUDA_CHECK_NOEXCEPT(cudaFreeHost(ptr));
    }
}

namespace async {

using Primitive = thesis::device::params::Primitive;

std::future<Result<std::vector<std::byte>>> readFileToBytes(const std::filesystem::path& filename) {
    return std::async(std::launch::async, [filename]() -> Result<std::vector<std::byte>> {
        return readFile(filename);
    });
}

std::future<Result<HDRImageData>> loadHDR(const std::filesystem::path& filename) {
    return std::async(std::launch::async,
                      [filename]() -> Result<HDRImageData> { return loadHDRImage(filename); });
}

std::future<Result<std::vector<Primitive>>> loadPrimitives(const std::filesystem::path& filename,
                                                           float sigma_multiplier,
                                                           float3 albedo_override) {
    return std::async(
        std::launch::async,
        [filename, sigma_multiplier, albedo_override]() -> Result<std::vector<Primitive>> {
            return loadPrimitivesFromPLY(filename, sigma_multiplier, albedo_override);
        });
}

std::future<Result<>> saveExr(cuda::AsyncBuffer<float4>&& buffer, size_t width, size_t height,
                              const std::filesystem::path& filename, bool flip_vertical) noexcept {
    return std::async(
        std::launch::async,
        [buf = std::move(buffer), width, height, filename, flip_vertical]() mutable -> Result<> {
            auto view = buf.host_view();
            return saveExrImage(view, width, height, filename, flip_vertical);
        });
}

}  // namespace async

}  // namespace thesis::host::utils::io
