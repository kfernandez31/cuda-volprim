#pragma once

#include "thesis/image_io.h"

#ifndef TINYEXR_IMPLEMENTATION
#define TINYEXR_IMPLEMENTATION
#endif

#include "tinyexr.h"

#include <array>
#include <fstream>
#include <stdexcept>

#define NUM_CHANNELS 3

namespace thesis {

// TODO: unnecessary alloc, just use stack mem
static const char* view_to_c_str(std::string_view view, std::string& out_str) {
    if (view.data()[view.size()] == '\0')
        return view.data();
    out_str = std::string(view);
    return out_str.c_str();
}

std::string read_ptx(std::string_view filename)
{
    std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open PTX file: " + std::string(filename));
    }

    std::string ptx(file.tellg(), '\0');
    file.seekg(0);
    file.read(ptx.data(), ptx.size());
    return ptx;
}

void save_exr_image(const std::vector<float3>& framebuffer, int width, int height, std::string_view filename, bool flip_vertical)
{
    constexpr std::array<const char*, NUM_CHANNELS> channel_names = { "B", "G", "R" };

    std::array<std::vector<float>, NUM_CHANNELS> channels;
    for (auto& chan : channels)
        chan.resize(width * height);

    for (int y = 0; y < height; ++y) {
        const size_t row_in  = y * width;
        const size_t row_out = (flip_vertical ? height - 1 - y : y) * width;

        for (int x = 0; x < width; ++x) {
            const auto& c = framebuffer[row_in + x];
            channels[0][row_out + x] = c.x; // R
            channels[1][row_out + x] = c.y; // G
            channels[2][row_out + x] = c.z; // B
        }
    }

    EXRImage image;
    InitEXRImage(&image);

    std::array<float*, NUM_CHANNELS> channel_ptrs = {
        channels[2].data(), // B
        channels[1].data(), // G
        channels[0].data(), // R
    };

    image.num_channels = NUM_CHANNELS;
    image.images = reinterpret_cast<unsigned char**>(channel_ptrs.data());
    image.width = width;
    image.height = height;

    EXRHeader header;
    InitEXRHeader(&header);

    std::array<EXRChannelInfo, NUM_CHANNELS> channelInfo;
    for (int i = 0; i < NUM_CHANNELS; ++i)
        strncpy(channelInfo[i].name, channel_names[i], 255);

    header.channels = channelInfo.data();
    header.num_channels = NUM_CHANNELS;

    std::array<int, NUM_CHANNELS> pixel_types;
    pixel_types.fill(TINYEXR_PIXELTYPE_FLOAT);
    header.pixel_types = pixel_types.data();
    header.requested_pixel_types = pixel_types.data();


    std::string _;
    auto filename_c_str = view_to_c_str(filename, _);

    const char* err = nullptr;
    if (SaveEXRImageToFile(&image, &header, filename_c_str, &err) != TINYEXR_SUCCESS) {
        // std::cerr << "Error saving EXR: " << err << std::endl; // TODO: log
        FreeEXRErrorMessage(err);
    } else {
        // std::cout << "Saved EXR: " << filename << "\n";  // TODO: log
    }
}

} // namespace thesis
