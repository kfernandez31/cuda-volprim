#include "thesis/host/utils/io.h"

#include "thesis/pch.h"

#include <vector_types.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <ios>
#include <span>
#include <stb/stb_image.h>
#include <string>
#include <tinyexr/tinyexr.h>
#include <vector>

#ifdef _MSC_VER
#include <cstdlib>
#endif  // _MSC_VER

namespace {

constexpr auto NUM_CHANNELS = 3u;
constexpr auto EXR_NAME_MAX_LEN = 255u;

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

}  // namespace

namespace thesis::host::utils::io {

Result<std::vector<std::byte>> readFileToBytes(const std::filesystem::path& filename) noexcept {
    try {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file) {
            return make_error("Failed to open file: {}", filename.string());
        }

        const auto fileSize = file.tellg();
        if (fileSize <= 0) {
            return make_error("File is empty or error reading file size: {}", filename.string());
        }

        std::vector<std::byte> buffer(static_cast<size_t>(fileSize));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        if (!file) {
            return make_error("Error while reading file: {}", filename.string());
        }

        return buffer;
    } catch (const std::exception& e) {
        return make_error("Exception in readFileToBytes: {}", e.what());
    }
}

Result<> saveExrImage(std::span<const float3> framebuffer, size_t width, size_t height,
                      const std::filesystem::path& filename, bool flip_vertical) noexcept {
    try {
        constexpr std::array<const char*, NUM_CHANNELS> channel_names = {"B", "G", "R"};

        std::array<std::vector<float>, NUM_CHANNELS> channels;
        for (auto& chan : channels) {
            chan.resize(width * height);
        }

        for (size_t y = 0; y < height; ++y) {
            const auto row_in = y * width;
            const auto row_out = (flip_vertical ? height - 1 - y : y) * width;

            for (size_t x = 0; x < width; ++x) {
                const auto& c = framebuffer[row_in + x];
                channels[0][row_out + x] = c.x;  // R
                channels[1][row_out + x] = c.y;  // G
                channels[2][row_out + x] = c.z;  // B
            }
        }

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

Result<HDRImagePtr> loadHDRImage(const std::filesystem::path& filename, size_t& width,
                                 size_t& height, size_t& channels) {
    spdlog::info("Loading environment map from '{}'", filename.string());

    stbi_set_flip_vertically_on_load(true);

    int w, h, c;
    auto* raw = stbi_loadf(filename.string().c_str(), &w, &h, &c, 0);
    if (!raw) {
        return make_error("Failed to load HDR image: {}", filename.string());
    }

    width = static_cast<size_t>(w);
    height = static_cast<size_t>(h);
    channels = static_cast<size_t>(c);

    return HDRImagePtr(raw, stbi_image_free);
}

Result<std::vector<params::Primitive>> loadPrimitives(const std::filesystem::path& filename) {
    try {
        happly::PLYData ply(filename.string());

        size_t N = 0;
        auto& vtx = ply.getElement("vertex");

        auto getProp = [&](const std::string& name) -> std::vector<float> {
            try {
                auto prop = vtx.getProperty<float>(name);
                if (N == 0) {
                    N = prop.size();
                } else if (prop.size() != N) {
                    throw std::runtime_error(
                        fmt::format("Expected size {}, got {}", N, prop.size()));
                }
                return prop;
            } catch (const std::exception& e) {
                throw std::runtime_error(fmt::format("Property \"{},\": {}", name, e.what()));
            }
        };

        auto sigma_t = getProp("sigma_t_0");
        N = sigma_t.size();

        auto p_x = getProp("x");
        auto p_y = getProp("y");
        auto p_z = getProp("z");

        auto rot_0 = getProp("rot_0");
        auto rot_1 = getProp("rot_1");
        auto rot_2 = getProp("rot_2");
        auto rot_3 = getProp("rot_3");

        auto scale_0 = getProp("scale_0");
        auto scale_1 = getProp("scale_1");
        auto scale_2 = getProp("scale_2");

        auto alb_0 = getProp("albedo_0");
        auto alb_1 = getProp("albedo_1");
        auto alb_2 = getProp("albedo_2");

        std::vector<params::Primitive> result;
        result.reserve(N);

        for (size_t i = 0; i < N; ++i) {
            const auto T = glm::translate(glm::vec3(p_x[i], p_y[i], p_z[i]));

            const auto q = glm::normalize(glm::quat(rot_0[i], rot_1[i], rot_2[i], rot_3[i]));
            const auto R = glm::toMat4(q);

            const auto S = glm::vec3(scale_0[i], scale_1[i], scale_2[i]);

            const glm::vec3 albedo(alb_0[i], alb_1[i], alb_2[i]);
            result.emplace_back(T, R, S, albedo, sigma_t[i]);
        }

        return result;

    } catch (const std::exception& e) {
        return make_error("Failed to load primitives from {}: {}", filename.string(), e.what());
    }
}

}  // namespace thesis::host::utils::io
