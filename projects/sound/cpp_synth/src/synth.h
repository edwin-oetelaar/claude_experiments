/**
 * @file synth.h
 * @brief A small polyphonic synthesizer: oscillators, ADSR envelopes, voices.
 *
 * Sound on a computer is a stream of numbers ("samples"): 48000 per second,
 * each the position of the loudspeaker cone between -1 and +1. A synthesizer
 * computes those numbers. Every pressed key gets a "voice": an oscillator that
 * produces a repeating wave at the key's frequency, multiplied by an envelope
 * that makes the note swell in and fade out instead of clicking on and off.
 *
 * Nothing here knows about SDL. render() fills a buffer; who plays it is not
 * this file's concern.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace synth {

enum class Waveform { Sine, Square, Saw, Triangle };

/** @brief Display name of a waveform, for the HUD. */
const char *waveform_name(Waveform w);

/** @brief MIDI note number to frequency in Hz: note 69 is A4 = 440 Hz, 12 notes per octave. */
float note_to_frequency(int midi_note);

/** @brief One period of @p w at @p phase (0..1), in the range -1..+1. */
float oscillator(Waveform w, float phase);

/** @brief Attack/decay/release in seconds, sustain as a level 0..1. */
struct Envelope {
    float attack = 0.01f;
    float decay = 0.20f;
    float sustain = 0.6f;
    float release = 0.40f;
};

class Synth {
public:
    static constexpr std::size_t kMaxVoices = 8;

    explicit Synth(float sample_rate);

    /** @brief Start @p note (MIDI number). Steals the oldest voice when all are busy. */
    void note_on(int note);
    /** @brief Release @p note; it fades out over the envelope's release time. */
    void note_off(int note);
    /** @brief Release every note. */
    void all_notes_off();

    void set_waveform(Waveform w) { waveform_ = w; }
    Waveform waveform() const { return waveform_; }
    Envelope &envelope() { return envelope_; }

    /** @brief Overwrite @p out with the next out.size() mono samples. */
    void render(std::span<float> out);

    /** @brief True while @p note is held (attack, decay or sustain; not fading out). */
    bool is_held(int note) const;

    /** @brief Voices that still make sound (including ones fading out). */
    std::size_t active_voices() const;
    float sample_rate() const { return sample_rate_; }

private:
    enum class Stage { Off, Attack, Decay, Sustain, Release };

    struct Voice {
        int note = -1;
        Stage stage = Stage::Off;
        float phase = 0.0f;     // position in the current period, 0..1
        float phase_step = 0.0f; // frequency / sample_rate
        float level = 0.0f;     // current envelope level, 0..1
        float release_step = 0.0f;
        std::uint64_t started = 0; // for stealing the oldest voice
    };

    float next_envelope_level(Voice &v) const;

    float sample_rate_;
    Waveform waveform_ = Waveform::Sine;
    Envelope envelope_;
    std::array<Voice, kMaxVoices> voices_{};
    std::uint64_t note_counter_ = 0;
};

} // namespace synth
