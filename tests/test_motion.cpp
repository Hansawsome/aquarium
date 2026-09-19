#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Motion.h"

using namespace aquarium;
using Catch::Approx;

static MotionParams P() {
    MotionParams p;
    p.maxSpeed = 100.f;
    p.accel = 200.f;      // units/s^2
    p.decel = 150.f;
    p.maxDeltaTime = 0.1f;
    return p;
}

TEST_CASE("accelerates toward max speed under input") {
    MotionState s;
    StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.x == Approx(20.f));       // 200 * 0.1
    StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.x == Approx(40.f));
}

TEST_CASE("velocity never exceeds max speed") {
    MotionState s;
    for (int i = 0; i < 100; ++i) StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.Length() <= Approx(100.f));
}

TEST_CASE("decelerates smoothly to zero when input released") {
    MotionState s;
    s.velocity = {30.f, 0.f};
    StepMotion(s, {0.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.x == Approx(15.f));       // 30 - 150*0.1
    StepMotion(s, {0.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.x == 0.f);                // no overshoot past zero
}

TEST_CASE("position integrates velocity") {
    MotionState s;
    s.velocity = {50.f, 0.f};
    StepMotion(s, {0.f, 0.f}, P(), 0.f);         // dt=0: nothing moves
    REQUIRE(s.position.x == 0.f);
    StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.position.x > 0.f);
}

TEST_CASE("huge dt is clamped (no teleport after focus loss)") {
    MotionState a, b;
    StepMotion(a, {1.f, 0.f}, P(), 5.0f);        // clamped to 0.1
    StepMotion(b, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(a.position.x == Approx(b.position.x));
    REQUIRE(a.velocity.x == Approx(b.velocity.x));
}

TEST_CASE("paused simulation does not change state") {
    MotionState s;
    s.velocity = {50.f, 0.f};
    s.paused = true;
    StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.position.x == 0.f);
    REQUIRE(s.velocity.x == Approx(50.f));
}
