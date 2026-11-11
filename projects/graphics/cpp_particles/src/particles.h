/**
 * @file particles.h
 * @brief A particle system: spawn, move under gravity, bounce, fade and die.
 *
 * The particles are stored as a "structure of arrays": one array per property
 * (x, y, vx, ...) instead of one array of Particle structs. The update loop
 * walks each array front to back, which is what the CPU cache likes best, and
 * it is the layout game engines use for exactly this kind of work.
 *
 * Nothing here knows about SDL. The renderer only reads the arrays.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace particles {

struct Vec2 {
    float x;
    float y;
};

/** @brief Tunable physics. Units: pixels and seconds; +y points down, as on screen. */
struct Params {
    float gravity = 900.0f;     /**< Downward acceleration in px/s^2. */
    float drag = 0.35f;         /**< Fraction of velocity lost per second to air resistance. */
    float bounce = 0.55f;       /**< Fraction of vertical speed kept after hitting the floor. */
    float speed_min = 450.0f;   /**< Launch speed range in px/s. */
    float speed_max = 1050.0f;
    float lifetime_min = 1.5f;  /**< Lifetime range in seconds. */
    float lifetime_max = 3.5f;
    float hue_speed = 40.0f;    /**< Degrees per second the colour of new particles rotates. */
};

class ParticleSystem {
public:
    /** @param capacity Maximum number of live particles; spawning beyond it is ignored. */
    explicit ParticleSystem(std::size_t capacity, std::uint32_t seed = 1);

    /**
     * @brief Launch up to @p count particles from @p origin.
     * @param angle  Launch direction in radians (0 = right, -pi/2 = straight up).
     * @param spread Total cone width in radians; 2*pi launches in every direction.
     * @return The number actually spawned (less than @p count when full).
     */
    std::size_t spawn(Vec2 origin, float angle, float spread, std::size_t count);

    /** @brief Advance the simulation by @p dt seconds; dead particles are removed. */
    void step(float dt);

    /** @brief Remove every particle. */
    void clear();

    /** @brief The floor particles bounce on, in px from the top. Infinite by default. */
    void set_floor(float floor_y) { floor_y_ = floor_y; }

    Params &params() { return params_; }
    const Params &params() const { return params_; }

    std::size_t size() const { return x_.size(); }
    std::size_t capacity() const { return capacity_; }

    std::span<const float> x() const { return x_; }
    std::span<const float> y() const { return y_; }
    std::span<const float> vx() const { return vx_; }
    std::span<const float> vy() const { return vy_; }
    std::span<const float> hue() const { return hue_; }

    /** @brief Remaining life of particle @p i as a fraction: 1 = just born, 0 = about to die. */
    float life_fraction(std::size_t i) const;

private:
    void remove(std::size_t i);
    float random(float lo, float hi);

    std::size_t capacity_;
    Params params_;
    float floor_y_;
    float hue_cursor_ = 0.0f;
    std::minstd_rand rng_;

    std::vector<float> x_, y_, vx_, vy_;
    std::vector<float> age_, lifetime_, hue_;
};

} // namespace particles
