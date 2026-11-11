/**
 * @file main.cpp
 * @brief SDL3 window, input and drawing around the particle simulation.
 *
 * The loop has the classic game-loop shape: handle input, advance the
 * simulation in fixed steps, draw. A fixed step (here 1/120 s) makes the
 * physics behave the same on a fast and a slow machine; only the number of
 * steps per frame changes.
 */

#include "particles.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <numbers>
#include <string_view>
#include <vector>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr std::size_t kMaxParticles = 60000;
constexpr float kSimStep = 1.0f / 120.0f; // seconds per physics step
constexpr float kMaxFrameTime = 0.25f;    // avoid a "spiral of death" after a hiccup
constexpr float kFountainRate = 2500.0f;  // particles per second
constexpr float kSprayRate = 5000.0f;
constexpr std::size_t kBurstCount = 4000;
constexpr float kParticleSize = 2.5f; // half the width of a particle quad, px

// RAII: the window and renderer are destroyed however main() returns.
struct WindowDeleter {
    void operator()(SDL_Window *w) const { SDL_DestroyWindow(w); }
};
struct RendererDeleter {
    void operator()(SDL_Renderer *r) const { SDL_DestroyRenderer(r); }
};
using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;

struct Options {
    long frames = 0;          // 0 = run until the window is closed
    const char *shot = nullptr; // write the last frame to this PNG
};

/** @brief Hue in degrees (0..360) to RGB, at full saturation and value. */
SDL_FColor hue_to_rgb(float hue)
{
    float h = hue / 60.0f;
    float x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
    switch (static_cast<int>(h) % 6) {
    case 0: return {1, x, 0, 1};
    case 1: return {x, 1, 0, 1};
    case 2: return {0, 1, x, 1};
    case 3: return {0, x, 1, 1};
    case 4: return {x, 0, 1, 1};
    default: return {1, 0, x, 1};
    }
}

/** @brief Spawn a whole number of particles per frame, carrying the fraction over. */
std::size_t take_whole(float &carry, float rate, float dt)
{
    carry += rate * dt;
    float whole = std::floor(carry);
    carry -= whole;
    return static_cast<std::size_t>(whole);
}

bool parse_args(int argc, char **argv, Options &opt)
{
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            opt.frames = std::strtol(argv[++i], nullptr, 10);
        } else if (arg == "--shot" && i + 1 < argc) {
            opt.shot = argv[++i];
        } else {
            std::printf("usage: %s [--frames N] [--shot FILE.png]\n"
                        "  --frames N  run N frames of 1/60 s, then quit\n"
                        "  --shot F    save the last frame as PNG\n",
                        argv[0]);
            return false;
        }
    }
    return true;
}

void save_shot(SDL_Renderer *renderer, const char *path)
{
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, nullptr);
    if (surface == nullptr || !SDL_SavePNG(surface, path)) {
        std::fprintf(stderr, "screenshot failed: %s\n", SDL_GetError());
    } else {
        std::printf("saved %s\n", path);
    }
    SDL_DestroySurface(surface);
}

/** @brief Turn every particle into a small quad (4 vertices, 6 indices). */
void build_mesh(const particles::ParticleSystem &ps, std::vector<SDL_Vertex> &verts,
                std::vector<int> &indices)
{
    verts.clear();
    indices.clear();
    auto x = ps.x();
    auto y = ps.y();
    auto hue = ps.hue();

    for (std::size_t i = 0; i < ps.size(); ++i) {
        SDL_FColor c = hue_to_rgb(hue[i]);
        // Additive blending: fading the colour towards black fades it out.
        float fade = ps.life_fraction(i);
        c = {c.r * fade, c.g * fade, c.b * fade, 1.0f};

        int base = static_cast<int>(verts.size());
        const float s = kParticleSize;
        verts.push_back({{x[i] - s, y[i] - s}, c, {0, 0}});
        verts.push_back({{x[i] + s, y[i] - s}, c, {0, 0}});
        verts.push_back({{x[i] + s, y[i] + s}, c, {0, 0}});
        verts.push_back({{x[i] - s, y[i] + s}, c, {0, 0}});
        for (int k : {0, 1, 2, 0, 2, 3}) {
            indices.push_back(base + k);
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        return EXIT_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    // Runs SDL_Quit when main returns, after the window and renderer are gone.
    struct SdlQuit {
        ~SdlQuit() { SDL_Quit(); }
    } sdl_quit;

    SDL_Window *raw_window = nullptr;
    SDL_Renderer *raw_renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Particles — left: spray, right: burst, G: gravity, C: clear",
                                     kWindowWidth, kWindowHeight, SDL_WINDOW_RESIZABLE,
                                     &raw_window, &raw_renderer)) {
        std::fprintf(stderr, "cannot create window: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    WindowPtr window(raw_window);
    RendererPtr renderer(raw_renderer);
    SDL_SetRenderVSync(renderer.get(), 1);
    // Overlapping particles add up to white: that is what makes it glow.
    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_ADD);

    particles::ParticleSystem ps(kMaxParticles);
    std::vector<SDL_Vertex> verts;
    std::vector<int> indices;
    verts.reserve(kMaxParticles * 4);
    indices.reserve(kMaxParticles * 6);

    constexpr float kUp = -std::numbers::pi_v<float> / 2.0f;
    constexpr float kFullCircle = 2.0f * std::numbers::pi_v<float>;
    bool fountain_on = true;
    float fountain_carry = 0.0f;
    float spray_carry = 0.0f;
    float accumulator = 0.0f;
    Uint64 last_ns = SDL_GetTicksNS();
    float fps = 0.0f;

    for (long frame = 1;; ++frame) {
        // --- input ---------------------------------------------------------
        bool quit = false;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                case SDLK_ESCAPE: quit = true; break;
                case SDLK_C: ps.clear(); break;
                case SDLK_F: fountain_on = !fountain_on; break;
                case SDLK_G: ps.params().gravity = ps.params().gravity > 0.0f ? 0.0f : 900.0f; break;
                default: break;
                }
            } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
                ps.spawn({e.button.x, e.button.y}, 0.0f, kFullCircle, kBurstCount);
            }
        }
        if (quit) {
            break;
        }

        // --- time ----------------------------------------------------------
        // With --frames every frame is exactly 1/60 s, so a smoke run or a
        // screenshot looks the same however fast the machine renders.
        Uint64 now_ns = SDL_GetTicksNS();
        float real_dt = static_cast<float>(now_ns - last_ns) * 1e-9f;
        last_ns = now_ns;
        float frame_dt = opt.frames > 0 ? 1.0f / 60.0f : std::min(real_dt, kMaxFrameTime);
        if (real_dt > 0.0f) {
            fps = 0.95f * fps + 0.05f * (1.0f / real_dt);
        }

        // --- simulate ------------------------------------------------------
        int w = 0;
        int h = 0;
        SDL_GetRenderOutputSize(renderer.get(), &w, &h);
        float width = static_cast<float>(w);
        float height = static_cast<float>(h);
        ps.set_floor(height - 8.0f);

        if (fountain_on) {
            ps.spawn({width * 0.5f, height - 8.0f}, kUp, 0.35f,
                     take_whole(fountain_carry, kFountainRate, frame_dt));
        }
        float mx = 0.0f;
        float my = 0.0f;
        if (SDL_GetMouseState(&mx, &my) & SDL_BUTTON_LMASK) {
            ps.spawn({mx, my}, kUp, 1.2f, take_whole(spray_carry, kSprayRate, frame_dt));
        }

        accumulator += frame_dt;
        while (accumulator >= kSimStep) {
            ps.step(kSimStep);
            accumulator -= kSimStep;
        }

        // --- draw ----------------------------------------------------------
        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());

        build_mesh(ps, verts, indices);
        SDL_RenderGeometry(renderer.get(), nullptr, verts.data(), static_cast<int>(verts.size()),
                           indices.data(), static_cast<int>(indices.size()));

        char hud[96];
        std::snprintf(hud, sizeof hud, "%zu particles   %.0f fps   fountain %s (F)", ps.size(),
                      static_cast<double>(fps), fountain_on ? "on" : "off");
        SDL_SetRenderDrawColor(renderer.get(), 200, 200, 200, 255);
        SDL_RenderDebugText(renderer.get(), 10.0f, 10.0f, hud);

        bool last_frame = opt.frames > 0 && frame >= opt.frames;
        if (last_frame && opt.shot != nullptr) {
            save_shot(renderer.get(), opt.shot); // must happen before present
        }
        SDL_RenderPresent(renderer.get());
        if (last_frame) {
            std::printf("ran %ld frames, %zu particles alive\n", frame, ps.size());
            break;
        }
    }
    return EXIT_SUCCESS;
}
