#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/SwimAnimation.h"

using namespace aquarium;
using Catch::Approx;

static SwimAnimParams P() {
    SwimAnimParams p;
    p.boneCount = 4;
    p.idleAmplitudeDeg = 2.f;
    p.amplitudePerSpeedDeg = 0.1f;   // deg per (unit/s)
    p.maxAmplitudeDeg = 15.f;
    p.idleFrequencyHz = 0.5f;
    p.frequencyPerSpeedHz = 0.01f;
    p.phaseStepRad = 0.8f;
    p.tailGain = 0.5f;
    p.bendPerTurnRateDeg = 0.2f;     // deg per (deg/s)
    p.maxBendDeg = 20.f;
    return p;
}

TEST_CASE("returns one angle per bone") {
    auto a = SwimAnimation::BoneAngles(0.f, 0.f, 0.f, P());
    REQUIRE(a.size() == 4);
}

TEST_CASE("stationary fish still breathes with idle amplitude") {
    // t chosen so sin(2π·0.5·t) = 1  ->  t = 0.5
    auto a = SwimAnimation::BoneAngles(0.f, 0.f, 0.5f, P());
    REQUIRE(a[0] == Approx(2.f));
}

TEST_CASE("amplitude grows with speed and is clamped") {
    auto slow = SwimAnimation::BoneAngles(50.f, 0.f, 0.f, P());   // freq = 1.0Hz -> sin(0)=0 at t=0
    auto fast = SwimAnimation::BoneAngles(1000.f, 0.f, 0.f, P());
    REQUIRE(SwimAnimation::Amplitude(50.f, P()) == Approx(7.f));      // 2 + 0.1*50
    REQUIRE(SwimAnimation::Amplitude(1000.f, P()) == Approx(15.f));   // clamped
    (void)slow; (void)fast;
}

TEST_CASE("tail bones lag in phase and swing wider") {
    const auto p = P();
    const float t = 0.25f;   // head: sin(2π·0.5·0.25) = sin(π/4)
    auto a = SwimAnimation::BoneAngles(0.f, 0.f, t, p);
    const float head = 2.f * std::sin(3.14159265f * 0.25f);
    const float tail = 2.f * (1.f + 3 * 0.5f) * std::sin(3.14159265f * 0.25f - 0.8f * 3);
    REQUIRE(a[0] == Approx(head).margin(1e-4f));
    REQUIRE(a[3] == Approx(tail).margin(1e-4f));
}

TEST_CASE("turning adds a clamped lateral bend to every bone") {
    const auto p = P();
    const auto base = SwimAnimation::BoneAngles(0.f, 0.f, 0.f, p);
    const auto turned = SwimAnimation::BoneAngles(0.f, 50.f, 0.f, p);      // bend = 0.2*50 = 10
    REQUIRE(turned[0] - base[0] == Approx(10.f));
    REQUIRE(turned[3] - base[3] == Approx(10.f));
    const auto clamped = SwimAnimation::BoneAngles(0.f, 500.f, 0.f, p);    // clamped to 20
    REQUIRE(clamped[0] - base[0] == Approx(20.f));
    const auto negative = SwimAnimation::BoneAngles(0.f, -500.f, 0.f, p);
    REQUIRE(negative[0] - base[0] == Approx(-20.f));
}
