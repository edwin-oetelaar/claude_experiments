/**
 * @file synth.cpp
 * @brief Naive oscillators, linear ADSR envelopes and voice allocation.
 */

#include "synth.h"

#include <cassert>
#include <cmath>
#include <numbers>

namespace synth {

namespace {
// Eight voices at full volume would add up to 8; scale down and then soft-clip.
constexpr float kMasterGain = 0.25f;
} // namespace

const char *waveform_name(Waveform w)
{
    switch (w) {
    case Waveform::Sine: return "sine";
    case Waveform::Square: return "square";
    case Waveform::Saw: return "saw";
    case Waveform::Triangle: return "triangle";
    }
    return "?";
}

float note_to_frequency(int midi_note)
{
    // Equal temperament: every semitone multiplies the frequency by 2^(1/12).
    return 440.0f * std::pow(2.0f, static_cast<float>(midi_note - 69) / 12.0f);
}

float oscillator(Waveform w, float phase)
{
    assert(phase >= 0.0f && phase < 1.0f);
    // These are "naive" waves: their sharp corners contain frequencies above
    // what 48 kHz can represent, which fold back as aliasing. Listen for it on
    // high saw notes; fixing it (PolyBLEP) is one of the exercises.
    switch (w) {
    case Waveform::Sine: return std::sin(2.0f * std::numbers::pi_v<float> * phase);
    case Waveform::Square: return phase < 0.5f ? 1.0f : -1.0f;
    case Waveform::Saw: return 2.0f * phase - 1.0f;
    case Waveform::Triangle: return 1.0f - 4.0f * std::fabs(phase - 0.5f);
    }
    return 0.0f;
}

Synth::Synth(float sample_rate) : sample_rate_(sample_rate)
{
    assert(sample_rate > 0.0f);
}

void Synth::note_on(int note)
{
    Voice *target = nullptr;
    // Same note again: restart that voice rather than stacking two.
    for (auto &v : voices_) {
        if (v.stage != Stage::Off && v.note == note) {
            target = &v;
        }
    }
    for (auto &v : voices_) {
        if (target == nullptr && v.stage == Stage::Off) {
            target = &v;
        }
    }
    if (target == nullptr) {
        target = &voices_[0];
        for (auto &v : voices_) {
            if (v.started < target->started) {
                target = &v;
            }
        }
    }

    target->note = note;
    target->stage = Stage::Attack;
    target->phase_step = note_to_frequency(note) / sample_rate_;
    target->started = ++note_counter_;
    // Keep phase and level: restarting a sounding voice from 0 would click.
}

void Synth::note_off(int note)
{
    for (auto &v : voices_) {
        if (v.note == note && v.stage != Stage::Off && v.stage != Stage::Release) {
            v.stage = Stage::Release;
            // Fade from wherever the level is now to 0 in exactly `release` seconds.
            v.release_step = v.level / (envelope_.release * sample_rate_);
        }
    }
}

void Synth::all_notes_off()
{
    for (auto &v : voices_) {
        if (v.stage != Stage::Off) {
            note_off(v.note);
        }
    }
}

float Synth::next_envelope_level(Voice &v) const
{
    switch (v.stage) {
    case Stage::Off:
        return 0.0f;
    case Stage::Attack:
        v.level += 1.0f / (envelope_.attack * sample_rate_);
        if (v.level >= 1.0f) {
            v.level = 1.0f;
            v.stage = Stage::Decay;
        }
        break;
    case Stage::Decay:
        v.level -= (1.0f - envelope_.sustain) / (envelope_.decay * sample_rate_);
        if (v.level <= envelope_.sustain) {
            v.level = envelope_.sustain;
            v.stage = Stage::Sustain;
        }
        break;
    case Stage::Sustain:
        v.level = envelope_.sustain;
        break;
    case Stage::Release:
        v.level -= v.release_step;
        if (v.level <= 0.0f) {
            v.level = 0.0f;
            v.stage = Stage::Off;
        }
        break;
    }
    return v.level;
}

void Synth::render(std::span<float> out)
{
    for (float &sample : out) {
        float mix = 0.0f;
        for (auto &v : voices_) {
            if (v.stage == Stage::Off) {
                continue;
            }
            mix += oscillator(waveform_, v.phase) * next_envelope_level(v);
            v.phase += v.phase_step;
            if (v.phase >= 1.0f) {
                v.phase -= 1.0f;
            }
        }
        // tanh is a gentle limiter: almost linear when quiet, never past +-1.
        sample = std::tanh(mix * kMasterGain);
    }
}

bool Synth::is_held(int note) const
{
    for (const auto &v : voices_) {
        if (v.note == note && v.stage != Stage::Off && v.stage != Stage::Release) {
            return true;
        }
    }
    return false;
}

std::size_t Synth::active_voices() const
{
    std::size_t n = 0;
    for (const auto &v : voices_) {
        n += v.stage != Stage::Off ? 1u : 0u;
    }
    return n;
}

} // namespace synth
