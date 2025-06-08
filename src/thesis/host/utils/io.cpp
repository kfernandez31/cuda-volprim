#include "thesis/host/utils/io.h"

#include "thesis/pch.h"

#include <vector_types.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
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

namespace thesis::host::io {

core::Result<std::string> readFileToString(const std::filesystem::path& filename) noexcept {
    try {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file) {
            return core::make_error("Failed to open file: {}", filename.string());
        }

        std::string ptx(file.tellg(), '\0');
        file.seekg(0);
        file.read(ptx.data(), static_cast<std::streamsize>(ptx.size()));

        return ptx;
    } catch (const std::exception& e) {
        return core::make_error("Exception in readFileToString: {}", e.what());
    }
}

core::Result<> saveExrImage(std::span<const float3> framebuffer, size_t width, size_t height,
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
            return core::make_error("EXR save failed: {}", err_msg);
        }

        return {};
    } catch (const std::exception& e) {
        return core::make_error("Exception in saveExrImage: {}", e.what());
    }
}

}  // namespace thesis::host::io
