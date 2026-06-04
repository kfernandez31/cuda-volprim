#include "thesis/host/utils/io.h"

#include "thesis/pch.h"

#include "internal.h"
#include "thesis/host/utils/check.h"
#include "thesis/host/utils/result.h"

#include <algorithm>
#include <charconv>

namespace {

using namespace thesis::host::utils;

// ─── Radiance .hdr (RGBE) loader ────────────────────────────────────────────
//
// Decodes Radiance picture files directly into pinned host memory, eliminating
// the pageable→pinned memcpy and the global flip flag that the previous stb-based
// loader required. Supports the two scanline encodings produced by every modern
// HDR encoder:
//   • new RLE (since Radiance 2.0): 4-byte scanline header (0x02 0x02 W_hi W_lo),
//     then 4 separately RLE-encoded byte channels (R, G, B, E).
//   • raw scanlines: width * 4 RGBE bytes verbatim, no compression.
// Pre-2.0 "old RLE" (the (1,1,1,N) repeat-shift escape) is not supported — flag
// it loudly if encountered.

constexpr size_t HDR_NUM_CHANNELS = 4;  // RGBA float, for CUDA texture compatibility
constexpr size_t HDR_MIN_RLE_WIDTH = 8;
constexpr size_t HDR_MAX_RLE_WIDTH = 0x7FFF;

inline void rgbeToRgba(uint8_t r, uint8_t g, uint8_t b, uint8_t e, float* out) noexcept {
    if (e == 0) {
        out[0] = out[1] = out[2] = 0.0f;
    } else {
        // Radiance: mantissa byte * 2^(exponent - 128) / 256 = byte * 2^(e - 136).
        // Match stb_image's convention (byte * f, not (byte+0.5) * f) for exact parity.
        const float f = std::ldexp(1.0f, static_cast<int>(e) - (128 + 8));
        out[0] = static_cast<float>(r) * f;
        out[1] = static_cast<float>(g) * f;
        out[2] = static_cast<float>(b) * f;
    }
    out[3] = 1.0f;
}

// Decode one RLE byte stream of `width` bytes into `out`. Each code byte is
// either >128 (run of (code & 0x7F) of the next byte) or 1..128 (literal copy
// of `code` following bytes). Advances `cursor` past the consumed bytes.
bool decodeRleChannel(const std::byte*& cursor, const std::byte* end, uint8_t* out,
                      size_t width) noexcept {
    size_t i = 0;
    while (i < width) {
        if (cursor >= end) {
            return false;
        }
        const uint8_t code = static_cast<uint8_t>(*cursor++);
        if (code > 128) {
            const size_t run_len = code & 0x7F;
            if (i + run_len > width || cursor >= end) {
                return false;
            }
            const uint8_t value = static_cast<uint8_t>(*cursor++);
            std::fill_n(out + i, run_len, value);
            i += run_len;
        } else {
            const size_t lit_len = code;
            if (lit_len == 0 || i + lit_len > width || cursor + lit_len > end) {
                return false;
            }
            for (size_t k = 0; k < lit_len; ++k) {
                out[i + k] = static_cast<uint8_t>(cursor[k]);
            }
            cursor += lit_len;
            i += lit_len;
        }
    }
    return true;
}

// Parse the ASCII header up to and including the resolution line. Sets width,
// height, and the byte offset where pixel data begins.
Result<> parseRgbeHeader(std::span<const std::byte> data, int& width, int& height,
                         size_t& pixel_offset) {
    // memchr is typically vectorized (SSE/AVX in glibc); faster than a hand-rolled
    // byte loop and reads better than `find_if`. Returns the index past '\n', or
    // data.size() if the line is unterminated (last line of file).
    auto consume_line = [&](size_t& pos) -> std::string_view {
        const std::byte* base = data.data();
        const auto* nl =
            static_cast<const std::byte*>(std::memchr(base + pos, '\n', data.size() - pos));
        const size_t start = pos;
        const size_t len = nl ? static_cast<size_t>(nl - (base + pos)) : data.size() - pos;
        pos = nl ? static_cast<size_t>(nl - base) + 1 : data.size();
        return std::string_view(reinterpret_cast<const char*>(base + start), len);
    };

    size_t pos = 0;
    const auto magic = consume_line(pos);
    if (magic != "#?RADIANCE" && magic != "#?RGBE") {
        return make_error("Not a Radiance HDR file (magic '{}')", std::string(magic));
    }

    bool format_ok = false;
    while (pos < data.size()) {
        const auto line = consume_line(pos);
        if (line.empty()) {
            break;  // blank line terminates the header
        }
        if (line.starts_with("FORMAT=")) {
            const auto fmt = line.substr(7);
            if (fmt == "32-bit_rle_rgbe") {
                format_ok = true;
            } else {
                return make_error("Unsupported HDR format '{}' (only 32-bit_rle_rgbe)",
                                  std::string(fmt));
            }
        }
        // Other key=value lines (EXPOSURE=, GAMMA=, SOFTWARE=, …) are ignored.
    }
    if (!format_ok) {
        return make_error("HDR file missing FORMAT line");
    }

    const auto resolution = consume_line(pos);
    // Standard environment-map orientation: "-Y H +X W" (rows top-down, pixels
    // left-to-right). Other axis orderings exist but are vanishingly rare for
    // env maps and adding them would be untestable speculation.
    if (!resolution.starts_with("-Y ")) {
        return make_error("Unsupported HDR axis orientation '{}' (expected '-Y H +X W')",
                          std::string(resolution));
    }
    const char* p = resolution.data() + 3;
    const char* e = resolution.data() + resolution.size();
    int h_val = 0;
    auto fc1 = std::from_chars(p, e, h_val);
    if (fc1.ec != std::errc{}) {
        return make_error("HDR resolution: bad height in '{}'", std::string(resolution));
    }
    // from_chars stops at the first non-digit; skip whitespace before "+X".
    const char* q = fc1.ptr;
    while (q < e && *q == ' ') {
        ++q;
    }
    if (e - q < 3 || std::string_view(q, 3) != "+X ") {
        return make_error("HDR resolution: expected ' +X W' after height in '{}'",
                          std::string(resolution));
    }
    int w_val = 0;
    auto fc2 = std::from_chars(q + 3, e, w_val);
    if (fc2.ec != std::errc{}) {
        return make_error("HDR resolution: bad width in '{}'", std::string(resolution));
    }
    if (w_val <= 0 || h_val <= 0) {
        return make_error("HDR file: invalid dimensions {}×{}", w_val, h_val);
    }

    width = w_val;
    height = h_val;
    pixel_offset = pos;
    return {};
}

Result<thesis::host::utils::io::HDRImageData> loadHDRImage(const std::filesystem::path& filename) {
    using namespace thesis::host::utils::io;
    spdlog::info("Loading environment map from '{}'", filename.string());

    // Slurp the file. HDR env maps are ~few-MB to ~30 MB; the costly part
    // (pinned alloc + pageable copy) is what we're avoiding by decoding straight
    // into pinned memory below.
    std::vector<std::byte> raw;
    TRY_ASSIGN(raw, io::detail::readFile(filename));

    int width_i = 0, height_i = 0;
    size_t pixel_offset = 0;
    TRY(parseRgbeHeader(raw, width_i, height_i, pixel_offset));
    const size_t width = static_cast<size_t>(width_i);
    const size_t height = static_cast<size_t>(height_i);

    // Pinned RGBA float output. Owned by HDRImagePtr from the moment it's
    // allocated so any early-return path frees it via CudaPinnedDeleter.
    const size_t total_bytes = width * height * HDR_NUM_CHANNELS * sizeof(float);
    float* pinned_raw = nullptr;
    cudaError_t err = cudaHostAlloc(&pinned_raw, total_bytes, cudaHostAllocDefault);
    if (err != cudaSuccess || !pinned_raw) {
        return make_error("Failed to allocate pinned memory for HDR image: {}",
                          cudaGetErrorString(err));
    }
    HDRImagePtr pinned(pinned_raw, CudaPinnedDeleter{});

    // Per-scanline RLE scratch: 4 byte-channels of `width` bytes each.
    std::vector<uint8_t> channel_buf(4 * width);
    uint8_t* const ch_r = channel_buf.data() + 0 * width;
    uint8_t* const ch_g = channel_buf.data() + 1 * width;
    uint8_t* const ch_b = channel_buf.data() + 2 * width;
    uint8_t* const ch_e = channel_buf.data() + 3 * width;

    const std::byte* p = raw.data() + pixel_offset;
    const std::byte* end = raw.data() + raw.size();

    // env-map convention: do NOT flip. A Radiance .hdr stores the TOP scanline
    // (sky/zenith) first, and env_map.sample() uses v = acos(y)/π so that y=+1 (up)
    // → v=0 → texture row 0. We therefore need row 0 to hold the FILE's first row
    // (sky). The previous flip_vertical=true ("preserve prior stb-with-flip
    // behavior") put the ground at row 0, rendering the environment upside-down.
    // That was invisible to every prior test — constant env (uniform), equatorial
    // +Z camera (the y-flip's fixed line), and an isotropic single Gaussian (whose
    // in-scatter integral is orientation-invariant) — first exposed by the
    // structured cloud under the real meadow HDR (FINDINGS §8.6/§8.7).
    constexpr bool flip_vertical = false;

    for (size_t y = 0; y < height; ++y) {
        const size_t out_row = flip_vertical ? height - 1 - y : y;
        float* out = pinned.get() + out_row * width * HDR_NUM_CHANNELS;

        if (p + 4 > end) {
            return make_error("HDR file: unexpected EOF at scanline {}/{}", y, height);
        }

        const uint8_t b0 = static_cast<uint8_t>(p[0]);
        const uint8_t b1 = static_cast<uint8_t>(p[1]);
        const uint8_t b2 = static_cast<uint8_t>(p[2]);
        const uint8_t b3 = static_cast<uint8_t>(p[3]);
        const size_t encoded_w = (static_cast<size_t>(b2) << 8) | b3;
        const bool is_new_rle = (b0 == 0x02 && b1 == 0x02 && (b2 & 0x80) == 0 &&
                                 encoded_w == width && width >= HDR_MIN_RLE_WIDTH &&
                                 width <= HDR_MAX_RLE_WIDTH);

        if (is_new_rle) {
            p += 4;
            if (!decodeRleChannel(p, end, ch_r, width) ||
                !decodeRleChannel(p, end, ch_g, width) ||
                !decodeRleChannel(p, end, ch_b, width) ||
                !decodeRleChannel(p, end, ch_e, width)) {
                return make_error("HDR file: corrupt RLE in scanline {}/{}", y, height);
            }
            for (size_t x = 0; x < width; ++x) {
                rgbeToRgba(ch_r[x], ch_g[x], ch_b[x], ch_e[x], out + x * HDR_NUM_CHANNELS);
            }
        } else if (b0 == 1 && b1 == 1 && b2 == 1) {
            // Pre-2.0 (1,1,1,N) repeat-shift escape — extremely rare today; refuse
            // rather than silently producing wrong pixels. Add support if a real
            // file ever needs it.
            return make_error(
                "HDR file uses legacy pre-2.0 RLE encoding which is not supported. "
                "Re-encode with a modern tool (Radiance ≥2.0, oiiotool, Blender)");
        } else {
            // Raw scanline: width*4 RGBE bytes verbatim (the 4 we already peeked
            // are the first pixel).
            if (p + width * 4 > end) {
                return make_error("HDR file: unexpected EOF in raw scanline {}/{}", y, height);
            }
            for (size_t x = 0; x < width; ++x) {
                rgbeToRgba(static_cast<uint8_t>(p[x * 4 + 0]),
                           static_cast<uint8_t>(p[x * 4 + 1]),
                           static_cast<uint8_t>(p[x * 4 + 2]),
                           static_cast<uint8_t>(p[x * 4 + 3]),
                           out + x * HDR_NUM_CHANNELS);
            }
            p += width * 4;
        }
    }

    return HDRImageData{.data = std::move(pinned),
                        .width = width,
                        .height = height,
                        .channels = HDR_NUM_CHANNELS};
}

}  // namespace

namespace thesis::host::utils::io {

void CudaPinnedDeleter::operator()(float* ptr) const noexcept {
    if (ptr) {
        CUDA_CHECK_NOEXCEPT(cudaFreeHost(ptr));
    }
}

namespace async {

std::future<Result<HDRImageData>> loadHDR(const std::filesystem::path& filename) {
    return std::async(std::launch::async,
                      [filename]() -> Result<HDRImageData> { return loadHDRImage(filename); });
}

}  // namespace async

}  // namespace thesis::host::utils::io
