/**
 * @file synth_test.cpp
 * @brief Tests for oscillators, envelopes, voices and the WAV writer, without a sound card.
 *
 * CHECK instead of assert(): assert() vanishes in a Release build (NDEBUG),
 * and the tests must check something in every build.
 */

#include "synth.h"
#include "wav.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int s_failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);          \
            ++s_failures;                                                                          \
        }                                                                                          \
    } while (0)

bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

using synth::Synth;
using synth::Waveform;

constexpr float kRate = 48000.0f;

void test_note_to_frequency()
{
    CHECK(near(synth::note_to_frequency(69), 440.0f, 1e-3f));  // A4
    CHECK(near(synth::note_to_frequency(81), 880.0f, 1e-2f));  // A5: one octave up doubles
    CHECK(near(synth::note_to_frequency(60), 261.63f, 1e-2f)); // C4
}

void test_oscillator_shapes()
{
    CHECK(near(synth::oscillator(Waveform::Sine, 0.25f), 1.0f, 1e-6f));
    CHECK(synth::oscillator(Waveform::Square, 0.1f) == 1.0f);
    CHECK(synth::oscillator(Waveform::Square, 0.6f) == -1.0f);
    CHECK(near(synth::oscillator(Waveform::Saw, 0.0f), -1.0f, 1e-6f));
    CHECK(near(synth::oscillator(Waveform::Triangle, 0.5f), 1.0f, 1e-6f));
    CHECK(near(synth::oscillator(Waveform::Triangle, 0.0f), -1.0f, 1e-6f));
}

/** Count upward zero crossings: one per period, so it measures the frequency. */
int rising_crossings(const std::vector<float> &v)
{
    int n = 0;
    for (std::size_t i = 1; i < v.size(); ++i) {
        n += (v[i - 1] <= 0.0f && v[i] > 0.0f) ? 1 : 0;
    }
    return n;
}

void test_pitch_is_right()
{
    Synth s(kRate);
    s.note_on(69); // 440 Hz
    std::vector<float> one_second(static_cast<std::size_t>(kRate));
    s.render(one_second);
    int periods = rising_crossings(one_second);
    CHECK(periods >= 439 && periods <= 441);
}

void test_silence_without_notes()
{
    Synth s(kRate);
    std::vector<float> buf(1000, 1.0f);
    s.render(buf);
    for (float x : buf) {
        CHECK(x == 0.0f);
    }
}

void test_envelope_reaches_sustain_and_releases()
{
    Synth s(kRate);
    s.set_waveform(Waveform::Square); // |sample| is then the envelope level, scaled
    s.envelope() = {0.01f, 0.1f, 0.5f, 0.2f};
    s.note_on(60);
    std::vector<float> buf(static_cast<std::size_t>(0.5f * kRate)); // well past attack + decay
    s.render(buf);
    float sustained = std::fabs(buf.back());
    CHECK(near(sustained, std::tanh(0.5f * 0.25f), 1e-4f)); // sustain * master gain, soft-clipped
    CHECK(s.active_voices() == 1);

    CHECK(s.is_held(60));
    s.note_off(60);
    CHECK(!s.is_held(60)); // released, though still fading out
    CHECK(s.active_voices() == 1);
    std::vector<float> release(static_cast<std::size_t>(0.25f * kRate)); // longer than release
    s.render(release);
    CHECK(s.active_voices() == 0);
    CHECK(release.back() == 0.0f);
}

void test_output_never_clips()
{
    Synth s(kRate);
    s.set_waveform(Waveform::Square);
    for (int n = 60; n < 68; ++n) {
        s.note_on(n);
    }
    std::vector<float> buf(4800);
    s.render(buf);
    for (float x : buf) {
        CHECK(x >= -1.0f && x <= 1.0f);
    }
}

void test_voice_stealing()
{
    Synth s(kRate);
    for (int n = 0; n < 12; ++n) { // more notes than voices
        s.note_on(60 + n);
    }
    CHECK(s.active_voices() == Synth::kMaxVoices);
    s.all_notes_off();
    std::vector<float> buf(static_cast<std::size_t>(kRate));
    s.render(buf);
    CHECK(s.active_voices() == 0);
}

void test_wav_header()
{
    const char *path = "synth_test.wav";
    std::vector<float> samples(100, 0.5f);
    CHECK(synth::write_wav(path, samples, 48000));

    std::FILE *f = std::fopen(path, "rb");
    CHECK(f != nullptr);
    if (f == nullptr) {
        return;
    }
    unsigned char h[44];
    CHECK(std::fread(h, 1, sizeof h, f) == sizeof h);
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fclose(f);
    std::remove(path);

    CHECK(std::memcmp(h, "RIFF", 4) == 0);
    CHECK(std::memcmp(h + 8, "WAVE", 4) == 0);
    CHECK(std::memcmp(h + 36, "data", 4) == 0);
    CHECK(size == 44 + 100 * 2);
    CHECK(h[24] == 0x80 && h[25] == 0xbb); // 48000 = 0xbb80, little-endian
}

} // namespace

int main()
{
    test_note_to_frequency();
    test_oscillator_shapes();
    test_pitch_is_right();
    test_silence_without_notes();
    test_envelope_reaches_sustain_and_releases();
    test_output_never_clips();
    test_voice_stealing();
    test_wav_header();

    if (s_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", s_failures);
        return EXIT_FAILURE;
    }
    std::printf("all synth tests passed\n");
    return EXIT_SUCCESS;
}
