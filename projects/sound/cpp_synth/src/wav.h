/**
 * @file wav.h
 * @brief Write mono float samples as a 16-bit PCM WAV file.
 *
 * WAV is a 44-byte header followed by the raw samples, little-endian. Writing
 * it by hand shows that an audio file is not magic: just numbers with a label.
 */

#pragma once

#include <span>

namespace synth {

/** @return true on success; false if the file cannot be written. */
bool write_wav(const char *path, std::span<const float> samples, int sample_rate);

} // namespace synth
