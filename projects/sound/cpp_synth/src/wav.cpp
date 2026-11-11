/**
 * @file wav.cpp
 * @brief Minimal RIFF/WAVE writer: one channel, 16 bit.
 */

#include "wav.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace synth {

namespace {

// WAV is little-endian. Writing byte by byte keeps this correct on any CPU.
void put_u16(std::FILE *f, std::uint16_t v)
{
    std::fputc(v & 0xff, f);
    std::fputc(v >> 8, f);
}

void put_u32(std::FILE *f, std::uint32_t v)
{
    put_u16(f, static_cast<std::uint16_t>(v & 0xffff));
    put_u16(f, static_cast<std::uint16_t>(v >> 16));
}

} // namespace

bool write_wav(const char *path, std::span<const float> samples, int sample_rate)
{
    std::FILE *f = std::fopen(path, "wb");
    if (f == nullptr) {
        return false;
    }

    const std::uint16_t channels = 1;
    const std::uint16_t bits = 16;
    const std::uint16_t block_align = channels * bits / 8;
    const auto rate = static_cast<std::uint32_t>(sample_rate);
    const auto data_bytes = static_cast<std::uint32_t>(samples.size() * block_align);

    std::fputs("RIFF", f);
    put_u32(f, 36 + data_bytes); // size of everything after this field
    std::fputs("WAVE", f);
    std::fputs("fmt ", f);
    put_u32(f, 16); // size of the fmt chunk
    put_u16(f, 1);  // 1 = PCM
    put_u16(f, channels);
    put_u32(f, rate);
    put_u32(f, rate * block_align); // bytes per second
    put_u16(f, block_align);
    put_u16(f, bits);
    std::fputs("data", f);
    put_u32(f, data_bytes);

    for (float s : samples) {
        float clamped = std::clamp(s, -1.0f, 1.0f);
        auto pcm = static_cast<std::int16_t>(std::lround(clamped * 32767.0f));
        put_u16(f, static_cast<std::uint16_t>(pcm));
    }

    bool ok = std::ferror(f) == 0;
    ok = std::fclose(f) == 0 && ok;
    return ok;
}

} // namespace synth
