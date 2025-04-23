#include "thesis/utils/io.h"

#include <third_party/spdlog/spdlog.h>
#include <third_party/tinyexr/tinyexr.h>

#include <array>
#include <cstddef>
#include <fstream>
#include <string>
#include <ios>

#ifdef _MSC_VER
#include <cstdlib>
#endif  // _MSC_VER

namespace {

constexpr size_t NUM_CHANNELS = 3;
constexpr size_t EXR_NAME_MAX_LEN = 255;

inline void safeStrncpy(char* dest, const char* src, size_t dest_size) {
#ifdef _MSC_VER
    strncpy_s(dest, dest_size, src, _TRUNCATE);
#else
    if (dest_size == 0) {
        return;
    }
    std::strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';  // ensure null-termination
#endif  // _MSC_VER
}

}  // namespace

namespace thesis::io {

std::optional<std::string> readFileToString(const std::string& filename) {
    std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);
    if (!file) {
        spdlog::error("Failed to open PTX file: {}", filename);
        return {};
    }

    std::string ptx(file.tellg(), '\0');
    file.seekg(0);
    file.read(ptx.data(), static_cast<std::streamsize>(ptx.size()));
    return ptx;
}

void saveExrImage(const std::vector<float3>& framebuffer, size_t width, size_t height,
                  const std::string& filename, bool flip_vertical) {
    constexpr std::array<const char*, NUM_CHANNELS> channel_names = {"B", "G", "R"};

    std::array<std::vector<float>, NUM_CHANNELS> channels;
    for (auto& chan : channels) {
        chan.resize(width * height);
    }

    for (size_t y = 0; y < height; ++y) {
        const size_t row_in = y * width;
        const size_t row_out = (flip_vertical ? height - 1 - y : y) * width;

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
    if (SaveEXRImageToFile(&image, &header, filename.c_str(), &err) != TINYEXR_SUCCESS) {
        spdlog::error("Error saving EXR: {}", err);
        FreeEXRErrorMessage(err);
    } else {
        spdlog::info("Saved EXR: {}", filename);
    }
}

}  // namespace thesis::io
