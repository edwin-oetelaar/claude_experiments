/**
 * @file particles.cpp
 * @brief Semi-implicit Euler integration with drag, a bouncing floor and swap-remove.
 */

#include "particles.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace particles {

ParticleSystem::ParticleSystem(std::size_t capacity, std::uint32_t seed)
    : capacity_(capacity), floor_y_(std::numeric_limits<float>::infinity()), rng_(seed)
{
    // Reserve once, so spawning never reallocates while the app runs.
    for (auto *v : {&x_, &y_, &vx_, &vy_, &age_, &lifetime_, &hue_}) {
        v->reserve(capacity);
    }
}

float ParticleSystem::random(float lo, float hi)
{
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

std::size_t ParticleSystem::spawn(Vec2 origin, float angle, float spread, std::size_t count)
{
    std::size_t room = capacity_ - size();
    std::size_t n = count < room ? count : room;

    for (std::size_t i = 0; i < n; ++i) {
        float a = angle + random(-0.5f * spread, 0.5f * spread);
        float speed = random(params_.speed_min, params_.speed_max);
        x_.push_back(origin.x);
        y_.push_back(origin.y);
        vx_.push_back(speed * std::cos(a));
        vy_.push_back(speed * std::sin(a));
        age_.push_back(0.0f);
        lifetime_.push_back(random(params_.lifetime_min, params_.lifetime_max));
        // A little jitter around the cursor so a burst is not one flat colour.
        hue_.push_back(std::fmod(hue_cursor_ + random(0.0f, 30.0f), 360.0f));
    }
    return n;
}

void ParticleSystem::step(float dt)
{
    assert(dt >= 0.0f);

    hue_cursor_ = std::fmod(hue_cursor_ + params_.hue_speed * dt, 360.0f);
    const float keep = std::max(0.0f, 1.0f - params_.drag * dt);

    // Walk backwards: remove() moves the last particle into slot i, and that
    // one has already been updated, so nothing is skipped or done twice.
    for (std::size_t i = size(); i-- > 0;) {
        age_[i] += dt;
        if (age_[i] >= lifetime_[i]) {
            remove(i);
            continue;
        }

        // Semi-implicit Euler: velocity first, then position with the new
        // velocity. Plain Euler (position first) slowly adds energy, so
        // bouncing particles would end up higher than they started.
        vy_[i] += params_.gravity * dt;
        vx_[i] *= keep;
        vy_[i] *= keep;
        x_[i] += vx_[i] * dt;
        y_[i] += vy_[i] * dt;

        if (y_[i] > floor_y_ && vy_[i] > 0.0f) {
            y_[i] = floor_y_;
            vy_[i] = -vy_[i] * params_.bounce;
        }
    }
}

void ParticleSystem::clear()
{
    for (auto *v : {&x_, &y_, &vx_, &vy_, &age_, &lifetime_, &hue_}) {
        v->clear();
    }
}

float ParticleSystem::life_fraction(std::size_t i) const
{
    assert(i < size());
    return 1.0f - age_[i] / lifetime_[i];
}

void ParticleSystem::remove(std::size_t i)
{
    assert(i < size());
    // Order does not matter for particles, so removal is O(1): copy the last
    // particle over this one and drop the last slot.
    for (auto *v : {&x_, &y_, &vx_, &vy_, &age_, &lifetime_, &hue_}) {
        (*v)[i] = v->back();
        v->pop_back();
    }
}

} // namespace particles
