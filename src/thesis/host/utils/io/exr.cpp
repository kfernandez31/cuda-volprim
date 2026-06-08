#include "thesis/host/utils/io.h"

#include "thesis/pch.h"

#include "thesis/host/cuda/async_buffer.h"
#include "thesis/host/utils/result.h"

#include <vector_types.h>

#include <algorithm>
#include <execution>
#include <numeric>
#include <ranges>
#include <tinyexr/tinyexr.h>

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

Result<> saveExrImage(std::span<const float4> framebuffer, size_t width, size_t height,
                      const std::filesystem::path& filename, bool flip_vertical) noexcept {
    try {
        // EXR convention: channels stored alphabetically (B, G, R). Index ↔ name ↔
        // data align directly here — channel i corresponds to channel_names[i] and
        // the i-th plane of `scratch`. Don't reorder one without the others.
        constexpr std::array<const char*, NUM_CHANNELS> channel_names = {"B", "G", "R"};

        // One contiguous scratch instead of three separate vectors: avoids two heap
        // allocations and two zero-fill passes (the buffers get fully overwritten
        // by the deinterleave below, so the value-init was pure waste).
        const size_t pixels = width * height;
        std::vector<float> scratch(NUM_CHANNELS * pixels);
        float* const b_plane = scratch.data();
        float* const g_plane = scratch.data() + pixels;
        float* const r_plane = scratch.data() + 2 * pixels;

        auto rows = std::views::iota(size_t{0}, height);
        std::for_each(std::execution::par, rows.begin(), rows.end(), [&](size_t y) {
            const auto row_in = y * width;
            const auto row_out = (flip_vertical ? height - 1 - y : y) * width;

            for (size_t x = 0; x < width; ++x) {
                const auto& c = framebuffer[row_in + x];
                b_plane[row_out + x] = c.z;
                g_plane[row_out + x] = c.y;
                r_plane[row_out + x] = c.x;  // W component ignored
            }
        });

        EXRImage image;
        InitEXRImage(&image);

        std::array<float*, NUM_CHANNELS> channel_ptrs = {b_plane, g_plane, r_plane};

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
            std::string err_msg(err ? err : "unknown error");
            FreeEXRErrorMessage(err);
            return make_error("EXR save failed: {}", err_msg);
        }

        return {};
    } catch (const std::exception& e) {
        return make_error("Exception in saveExrImage: {}", e.what());
    }
}

}  // namespace

namespace thesis::host::utils::io::async {

std::future<Result<>> saveExr(cuda::AsyncBuffer<float4>&& buffer, size_t width, size_t height,
                              const std::filesystem::path& filename, bool flip_vertical) noexcept {
    return std::async(
        std::launch::async,
        [buf = std::move(buffer), width, height, filename, flip_vertical]() mutable -> Result<> {
            auto view = buf.host_view();
            return saveExrImage(view, width, height, filename, flip_vertical);
        });
}

}  // namespace thesis::host::utils::io::async
