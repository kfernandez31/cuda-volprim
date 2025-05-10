#pragma once

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#define NUM_CHANNELS 3

void save_exr_image(const std::vector<vec3>& framebuffer, int width, int height, const std::string& filename, bool flip_vertical = true)
{
    constexpr std::array<const char*, NUM_CHANNELS> channel_names = { "B", "G", "R" };

    std::array<std::vector<float>, NUM_CHANNELS> channels;
    for (auto& chan : channels)
        chan.resize(width * height);

    for (int y = 0; y < height; ++y) {
        const size_t row_in  = y * width;
        const size_t row_out = (flip_vertical ? height - 1 - y : y) * width;

        for (int x = 0; x < width; ++x) {
            const vec3& c = framebuffer[row_in + x];
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

    const char* err = nullptr;
    if (SaveEXRImageToFile(&image, &header, filename.c_str(), &err) != TINYEXR_SUCCESS) {
        std::cerr << "Error saving EXR: " << err << std::endl;
        FreeEXRErrorMessage(err);
    } else {
        std::cout << "Saved EXR: " << filename << "\n";
    }
}
