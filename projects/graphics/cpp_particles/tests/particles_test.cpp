/**
 * @file particles_test.cpp
 * @brief Tests for the simulation, without a window.
 *
 * CHECK instead of assert(): assert() vanishes in a Release build (NDEBUG),
 * and the tests must check something in every build.
 */

#include "particles.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>

namespace {

int s_failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);          \
            ++s_failures;                                                                          \
        }                                                                                          \
    } while (0)

using particles::ParticleSystem;

void test_spawn_respects_capacity()
{
    ParticleSystem ps(100);
    CHECK(ps.spawn({0, 0}, 0.0f, 1.0f, 60) == 60);
    CHECK(ps.spawn({0, 0}, 0.0f, 1.0f, 60) == 40); // only 40 left
    CHECK(ps.size() == 100);
    CHECK(ps.spawn({0, 0}, 0.0f, 1.0f, 1) == 0);
}

void test_gravity_pulls_down()
{
    ParticleSystem ps(1);
    ps.params().drag = 0.0f;
    ps.spawn({0, 0}, 0.0f, 0.0f, 1); // straight to the right: vy starts at 0
    CHECK(std::fabs(ps.vy()[0]) < 1e-3f);
    ps.step(0.1f);
    // One step of semi-implicit Euler: vy = g*dt, y = vy*dt.
    CHECK(std::fabs(ps.vy()[0] - 90.0f) < 1e-3f);
    CHECK(std::fabs(ps.y()[0] - 9.0f) < 1e-3f);
}

void test_particles_die_after_lifetime()
{
    ParticleSystem ps(1000);
    ps.params().lifetime_min = 1.0f;
    ps.params().lifetime_max = 2.0f;
    ps.spawn({0, 0}, 0.0f, 1.0f, 1000);
    for (int i = 0; i < 100; ++i) {
        ps.step(0.01f); // 1 s
    }
    CHECK(ps.size() <= 1000);
    CHECK(ps.size() > 0); // some live up to 2 s
    for (int i = 0; i < 101; ++i) {
        ps.step(0.01f); // just past 2 s
    }
    CHECK(ps.size() == 0);
}

void test_floor_bounces()
{
    ParticleSystem ps(1);
    ps.params().drag = 0.0f;
    ps.params().lifetime_min = ps.params().lifetime_max = 100.0f;
    ps.set_floor(100.0f);
    ps.spawn({0, 0}, std::numbers::pi_v<float> / 2.0f, 0.0f, 1); // straight down
    for (int i = 0; i < 1000; ++i) {
        ps.step(0.001f);
        CHECK(ps.y()[0] <= 100.0f);
    }
}

void test_life_fraction()
{
    ParticleSystem ps(1);
    ps.params().lifetime_min = ps.params().lifetime_max = 2.0f;
    ps.spawn({0, 0}, 0.0f, 0.0f, 1);
    CHECK(std::fabs(ps.life_fraction(0) - 1.0f) < 1e-6f);
    ps.step(1.0f);
    CHECK(std::fabs(ps.life_fraction(0) - 0.5f) < 1e-6f);
}

void test_same_seed_same_result()
{
    ParticleSystem a(500, 42);
    ParticleSystem b(500, 42);
    a.spawn({10, 10}, 0.0f, 3.0f, 500);
    b.spawn({10, 10}, 0.0f, 3.0f, 500);
    a.step(0.5f);
    b.step(0.5f);
    CHECK(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a.x()[i] == b.x()[i]);
    }
}

} // namespace

int main()
{
    test_spawn_respects_capacity();
    test_gravity_pulls_down();
    test_particles_die_after_lifetime();
    test_floor_bounces();
    test_life_fraction();
    test_same_seed_same_result();

    if (s_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", s_failures);
        return EXIT_FAILURE;
    }
    std::printf("all particle tests passed\n");
    return EXIT_SUCCESS;
}
