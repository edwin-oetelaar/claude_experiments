/**
 * @file main.cpp
 * @brief Keyboard input, SDL3 audio output and an oscilloscope view.
 *
 * Two threads share the synth. SDL calls audio_callback() on its own audio
 * thread whenever the sound card needs more samples; the main thread handles
 * keys and draws. The audio stream's lock keeps them apart: SDL holds it while
 * the callback runs, and the main thread takes it (SDL_LockAudioStream) before
 * it touches the synth or reads the scope buffer.
 *
 * Rule of thumb for the callback: be quick, never wait. If it is late, the
 * sound card runs dry and you hear a click.
 */

#include "synth.h"
#include "wav.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <vector>

namespace {

constexpr int kSampleRate = 48000;
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 600;
constexpr std::size_t kScopeSize = 4096;  // recent samples kept for drawing
constexpr std::size_t kScopeShown = 1024; // samples across the screen (~21 ms)
constexpr int kBaseNote = 60;             // C4 on the A key

using synth::Synth;
using synth::Waveform;

struct WindowDeleter {
    void operator()(SDL_Window *w) const { SDL_DestroyWindow(w); }
};
struct RendererDeleter {
    void operator()(SDL_Renderer *r) const { SDL_DestroyRenderer(r); }
};
struct StreamDeleter {
    void operator()(SDL_AudioStream *s) const { SDL_DestroyAudioStream(s); }
};
using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;
using StreamPtr = std::unique_ptr<SDL_AudioStream, StreamDeleter>;

/** @brief Everything the audio thread touches. Guarded by the audio stream lock. */
struct AudioState {
    Synth synth{static_cast<float>(kSampleRate)};
    std::array<float, kScopeSize> scope{}; // ring buffer of the latest output
    std::size_t scope_pos = 0;
};

/** @brief One piano key: physical key position (scancode), semitone, label. */
struct Key {
    SDL_Scancode scancode;
    int semitone;
    bool black;
    const char *label;
};

// Scancodes, not key codes: the keys sit in the same place on QWERTY, AZERTY, ...
constexpr std::array<Key, 13> kKeys = {{
    {SDL_SCANCODE_A, 0, false, "A"},  {SDL_SCANCODE_W, 1, true, "W"},
    {SDL_SCANCODE_S, 2, false, "S"},  {SDL_SCANCODE_E, 3, true, "E"},
    {SDL_SCANCODE_D, 4, false, "D"},  {SDL_SCANCODE_F, 5, false, "F"},
    {SDL_SCANCODE_T, 6, true, "T"},   {SDL_SCANCODE_G, 7, false, "G"},
    {SDL_SCANCODE_Y, 8, true, "Y"},   {SDL_SCANCODE_H, 9, false, "H"},
    {SDL_SCANCODE_U, 10, true, "U"},  {SDL_SCANCODE_J, 11, false, "J"},
    {SDL_SCANCODE_K, 12, false, "K"},
}};

// --- demo song ----------------------------------------------------------------

constexpr float kDemoStep = 0.2f; // seconds per note
constexpr std::array<int, 16> kDemoNotes = {60, 64, 67, 72, 57, 60, 64, 69,
                                            53, 57, 60, 65, 55, 59, 62, 67}; // C Am F G
constexpr float kDemoLoop = kDemoStep * static_cast<float>(kDemoNotes.size());

/**
 * @brief Send the demo's note events that fall in (t_prev, t_now].
 *
 * Every pass through the song uses the next waveform, so the demo plays all four.
 */
void demo_events(Synth &s, float t_prev, float t_now)
{
    auto crossed = [&](float t) { return t > t_prev && t <= t_now; };
    for (int loop = static_cast<int>(t_prev / kDemoLoop); loop <= static_cast<int>(t_now / kDemoLoop);
         ++loop) {
        float loop_start = static_cast<float>(loop) * kDemoLoop;
        for (std::size_t i = 0; i < kDemoNotes.size(); ++i) {
            float on = loop_start + static_cast<float>(i) * kDemoStep;
            if (crossed(on)) {
                if (i == 0) {
                    s.set_waveform(static_cast<Waveform>(loop % 4));
                }
                s.note_on(kDemoNotes[i]);
            }
            if (crossed(on + 0.75f * kDemoStep)) {
                s.note_off(kDemoNotes[i]);
            }
        }
    }
}

/** @brief Render four passes of the demo straight into a WAV file. */
int render_demo_wav(const char *path)
{
    Synth s(static_cast<float>(kSampleRate));
    const float seconds = 4.0f * kDemoLoop + 1.0f; // plus the last release
    std::vector<float> samples(static_cast<std::size_t>(seconds * kSampleRate));

    // Render in blocks of 5 ms, just like a sound card asks for blocks.
    constexpr std::size_t kBlock = kSampleRate / 200;
    for (std::size_t pos = 0; pos < samples.size(); pos += kBlock) {
        // Start just below 0, so the event at t = 0 falls inside the first block.
        float t_prev = pos == 0 ? -1.0f : static_cast<float>(pos) / kSampleRate;
        float t_now = static_cast<float>(pos + kBlock) / kSampleRate;
        demo_events(s, t_prev, std::min(t_now, 4.0f * kDemoLoop - 0.001f));
        std::size_t n = std::min(kBlock, samples.size() - pos);
        s.render(std::span<float>(samples.data() + pos, n));
    }

    if (!synth::write_wav(path, samples, kSampleRate)) {
        std::fprintf(stderr, "cannot write %s\n", path);
        return EXIT_FAILURE;
    }
    std::printf("wrote %s (%.1f s)\n", path, static_cast<double>(seconds));
    return EXIT_SUCCESS;
}

// --- audio thread -------------------------------------------------------------

/** @brief Called by SDL on the audio thread, with the stream locked. */
void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount,
                            int /*total_amount*/)
{
    auto *state = static_cast<AudioState *>(userdata);
    std::array<float, 512> block;
    int needed = additional_amount / static_cast<int>(sizeof(float));

    while (needed > 0) {
        int n = std::min(needed, static_cast<int>(block.size()));
        std::span<float> chunk(block.data(), static_cast<std::size_t>(n));
        state->synth.render(chunk);
        for (float s : chunk) {
            state->scope[state->scope_pos] = s;
            state->scope_pos = (state->scope_pos + 1) % kScopeSize;
        }
        SDL_PutAudioStreamData(stream, block.data(), n * static_cast<int>(sizeof(float)));
        needed -= n;
    }
}

// --- drawing ------------------------------------------------------------------

/**
 * @brief Draw the latest output like an oscilloscope.
 *
 * A scope "triggers": it starts drawing where the wave crosses zero going up.
 * Without that, every frame starts at a random point and the wave jitters.
 */
void draw_scope(SDL_Renderer *r, const std::vector<float> &samples, float width, float top,
                float height)
{
    std::size_t start = samples.size() - kScopeShown;
    std::size_t earliest = samples.size() - 2 * kScopeShown;
    for (std::size_t i = start; i > earliest; --i) {
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f) {
            start = i;
            break;
        }
    }

    std::array<SDL_FPoint, kScopeShown> points;
    float mid = top + height * 0.5f;
    for (std::size_t i = 0; i < kScopeShown; ++i) {
        points[i].x = width * static_cast<float>(i) / static_cast<float>(kScopeShown - 1);
        points[i].y = mid - samples[start + i] * height * 0.45f;
    }

    SDL_SetRenderDrawColor(r, 40, 60, 40, 255);
    SDL_RenderLine(r, 0.0f, mid, width, mid);
    SDL_SetRenderDrawColor(r, 80, 255, 120, 255);
    SDL_RenderLines(r, points.data(), static_cast<int>(points.size()));
}

void draw_keyboard(SDL_Renderer *r, const std::array<bool, kKeys.size()> &lit, float width,
                   float top, float height)
{
    const float white_w = width / 8.0f;
    const float black_w = white_w * 0.6f;
    int white_index = 0;
    std::array<SDL_FRect, kKeys.size()> rects;
    for (std::size_t k = 0; k < kKeys.size(); ++k) {
        if (kKeys[k].black) {
            // A black key sits on the border between the previous and next white key.
            rects[k] = {static_cast<float>(white_index) * white_w - black_w / 2, top, black_w,
                        height * 0.6f};
        } else {
            rects[k] = {static_cast<float>(white_index) * white_w + 1, top, white_w - 2, height};
            ++white_index;
        }
    }
    // White keys first, black keys on top.
    for (bool black : {false, true}) {
        for (std::size_t k = 0; k < kKeys.size(); ++k) {
            if (kKeys[k].black != black) {
                continue;
            }
            bool down = lit[k];
            if (black) {
                SDL_SetRenderDrawColor(r, down ? 60 : 20, down ? 200 : 20, down ? 90 : 20, 255);
            } else {
                SDL_SetRenderDrawColor(r, down ? 120 : 230, down ? 255 : 230, down ? 150 : 230, 255);
            }
            SDL_RenderFillRect(r, &rects[k]);
            SDL_SetRenderDrawColor(r, black ? 200 : 40, black ? 200 : 40, black ? 200 : 40, 255);
            SDL_RenderDebugText(r, rects[k].x + rects[k].w / 2 - 4,
                                rects[k].y + rects[k].h - 16, kKeys[k].label);
        }
    }
}

struct Options {
    long frames = 0;
    const char *shot = nullptr;
    const char *wav = nullptr;
    bool demo = false;
};

bool parse_args(int argc, char **argv, Options &opt)
{
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            opt.frames = std::strtol(argv[++i], nullptr, 10);
        } else if (arg == "--shot" && i + 1 < argc) {
            opt.shot = argv[++i];
        } else if (arg == "--wav" && i + 1 < argc) {
            opt.wav = argv[++i];
        } else if (arg == "--demo") {
            opt.demo = true;
        } else {
            std::printf("usage: %s [--demo] [--frames N] [--shot FILE.png] [--wav FILE.wav]\n"
                        "  --demo      play a short song by itself\n"
                        "  --frames N  run N frames (60 per second), then quit\n"
                        "  --shot F    save the last frame as PNG\n"
                        "  --wav F     render the demo to a WAV file, no window or sound card\n",
                        argv[0]);
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        return EXIT_FAILURE;
    }
    if (opt.wav != nullptr) {
        return render_demo_wav(opt.wav);
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    struct SdlQuit {
        ~SdlQuit() { SDL_Quit(); }
    } sdl_quit;

    SDL_Window *raw_window = nullptr;
    SDL_Renderer *raw_renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Synth — keys A..K play, 1-4 waveform, Z/X octave",
                                     kWindowWidth, kWindowHeight, 0, &raw_window, &raw_renderer)) {
        std::fprintf(stderr, "cannot create window: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    WindowPtr window(raw_window);
    RendererPtr renderer(raw_renderer);
    SDL_SetRenderVSync(renderer.get(), 1);

    // Declared after the window but destroyed before it: the stream (and with
    // it the audio thread) must be gone before `audio` goes out of scope.
    AudioState audio;
    SDL_AudioSpec spec{SDL_AUDIO_F32, 1, kSampleRate};
    StreamPtr stream(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                               audio_callback, &audio));
    if (!stream) {
        std::fprintf(stderr,
                     "cannot open audio: %s\n"
                     "(no sound card? try SDL_AUDIODRIVER=dummy, or --wav to render a file)\n",
                     SDL_GetError());
        return EXIT_FAILURE;
    }
    SDL_ResumeAudioStreamDevice(stream.get());

    std::array<int, kKeys.size()> held; // MIDI note per key while held, else -1
    held.fill(-1);
    int octave_shift = 0;
    std::vector<float> scope(kScopeSize);
    Uint64 start_ns = SDL_GetTicksNS();
    float demo_t = 0.0f;

    for (long frame = 1;; ++frame) {
        Uint64 frame_start = SDL_GetTicksNS();
        bool quit = false;

        // Everything that touches the synth happens with the stream locked.
        SDL_LockAudioStream(stream.get());
        Synth &s = audio.synth;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                switch (e.key.scancode) {
                case SDL_SCANCODE_ESCAPE: quit = true; break;
                case SDL_SCANCODE_1: s.set_waveform(Waveform::Sine); break;
                case SDL_SCANCODE_2: s.set_waveform(Waveform::Square); break;
                case SDL_SCANCODE_3: s.set_waveform(Waveform::Saw); break;
                case SDL_SCANCODE_4: s.set_waveform(Waveform::Triangle); break;
                case SDL_SCANCODE_Z: octave_shift = std::max(octave_shift - 1, -3); break;
                case SDL_SCANCODE_X: octave_shift = std::min(octave_shift + 1, 3); break;
                default:
                    for (std::size_t k = 0; k < kKeys.size(); ++k) {
                        if (kKeys[k].scancode == e.key.scancode && held[k] < 0) {
                            held[k] = kBaseNote + 12 * octave_shift + kKeys[k].semitone;
                            s.note_on(held[k]);
                        }
                    }
                    break;
                }
            } else if (e.type == SDL_EVENT_KEY_UP) {
                for (std::size_t k = 0; k < kKeys.size(); ++k) {
                    // Release the note this key started, even if the octave changed since.
                    if (kKeys[k].scancode == e.key.scancode && held[k] >= 0) {
                        s.note_off(held[k]);
                        held[k] = -1;
                    }
                }
            }
        }
        if (opt.demo) {
            float t = opt.frames > 0 ? static_cast<float>(frame) / 60.0f
                                     : static_cast<float>(SDL_GetTicksNS() - start_ns) * 1e-9f;
            demo_events(s, demo_t, t);
            demo_t = t;
        }
        // Copy the ring buffer out oldest-first, so drawing needs no lock.
        for (std::size_t i = 0; i < kScopeSize; ++i) {
            scope[i] = audio.scope[(audio.scope_pos + i) % kScopeSize];
        }
        // Light a key when it sounds, whether you pressed it or the demo did.
        std::array<bool, kKeys.size()> lit{};
        for (std::size_t k = 0; k < kKeys.size(); ++k) {
            lit[k] = s.is_held(kBaseNote + 12 * octave_shift + kKeys[k].semitone);
        }
        const char *wave_name = synth::waveform_name(s.waveform());
        std::size_t voices = s.active_voices();
        SDL_UnlockAudioStream(stream.get());

        if (quit) {
            break;
        }

        SDL_SetRenderDrawColor(renderer.get(), 10, 14, 10, 255);
        SDL_RenderClear(renderer.get());
        draw_scope(renderer.get(), scope, kWindowWidth, 40.0f, 340.0f);
        draw_keyboard(renderer.get(), lit, kWindowWidth, 400.0f, 190.0f);

        char hud[128];
        std::snprintf(hud, sizeof hud, "wave: %-8s (1-4)   octave: C%d (Z/X)   voices: %zu/%zu%s",
                      wave_name, 4 + octave_shift, voices, Synth::kMaxVoices,
                      opt.demo ? "   [demo]" : "");
        SDL_SetRenderDrawColor(renderer.get(), 200, 200, 200, 255);
        SDL_RenderDebugText(renderer.get(), 10.0f, 12.0f, hud);

        bool last_frame = opt.frames > 0 && frame >= opt.frames;
        if (last_frame && opt.shot != nullptr) {
            SDL_Surface *shot = SDL_RenderReadPixels(renderer.get(), nullptr);
            if (shot == nullptr || !SDL_SavePNG(shot, opt.shot)) {
                std::fprintf(stderr, "screenshot failed: %s\n", SDL_GetError());
            } else {
                std::printf("saved %s\n", opt.shot);
            }
            SDL_DestroySurface(shot);
        }
        SDL_RenderPresent(renderer.get());
        if (last_frame) {
            std::printf("ran %ld frames\n", frame);
            break;
        }

        // Without a display there is no vsync; hold 60 fps so the audio keeps pace.
        Uint64 frame_ns = SDL_GetTicksNS() - frame_start;
        if (opt.frames > 0 && frame_ns < SDL_NS_PER_SECOND / 60) {
            SDL_DelayNS(SDL_NS_PER_SECOND / 60 - frame_ns);
        }
    }
    return EXIT_SUCCESS;
}
