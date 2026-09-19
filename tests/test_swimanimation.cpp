#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "aquarium/SwimAnimation.h"

using namespace aquarium;
using Catch::Approx;

static constexpr float kPi = 3.14159265f;
static constexpr float kTwoPi = 6.28318530f;

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
    // phase chosen so sin(phase) = 1  ->  phase = 2π·0.5·0.5 = π/2
    const float phase = kTwoPi * 0.5f * 0.5f;
    auto a = SwimAnimation::BoneAngles(0.f, 0.f, phase, P());
    REQUIRE(a[0] == Approx(2.f));
}

TEST_CASE("amplitude grows with speed and is clamped") {
    REQUIRE(SwimAnimation::Amplitude(50.f, P()) == Approx(7.f));      // 2 + 0.1*50
    REQUIRE(SwimAnimation::Amplitude(1000.f, P()) == Approx(15.f));   // clamped
    // phase = 0 -> sin(0) = 0, so speed only affects amplitude (which is multiplied by 0 here)
    REQUIRE(SwimAnimation::BoneAngles(50.f, 0.f, 0.f, P())[0] == Approx(0.f).margin(1e-5f));
}

TEST_CASE("tail bones lag in phase and swing wider") {
    const auto p = P();
    // phase = 2π·0.5·0.25 = π/4 (idle frequency, t = 0.25s equivalent)
    const float phase = kTwoPi * 0.5f * 0.25f;
    auto a = SwimAnimation::BoneAngles(0.f, 0.f, phase, p);
    const float head = 2.f * std::sin(kPi * 0.25f);
    const float tail = 2.f * (1.f + 3 * 0.5f) * std::sin(kPi * 0.25f - 0.8f * 3);
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

TEST_CASE("phase advances by 2pi*Frequency*dt and wraps") {
    const auto p = P();
    REQUIRE(SwimAnimation::AdvancePhase(0.f, 0.f, 1.0f, p) == Approx(kPi));                 // idle 0.5Hz
    REQUIRE(SwimAnimation::AdvancePhase(6.0f, 0.f, 1.0f, p) == Approx(6.0f + kPi - kTwoPi));
    REQUIRE(SwimAnimation::AdvancePhase(1.0f, 0.f, 0.0f, p) == 1.0f);
    REQUIRE(SwimAnimation::AdvancePhase(1.0f, 0.f, -0.1f, p) == 1.0f);
}

TEST_CASE("changing speed does not jump the phase") {
    const auto p = P();
    // speed=50 keeps the step below one full turn (Frequency=1.0Hz, delta~=0.628 rad), so no wrap occurs.
    const float p1 = SwimAnimation::AdvancePhase(0.f, 0.f, 0.1f, p);
    const float p2 = SwimAnimation::AdvancePhase(p1, 50.f, 0.1f, p);
    REQUIRE(p2 - p1 == Approx(kTwoPi * SwimAnimation::Frequency(50.f, p) * 0.1f));
}

TEST_CASE("negative speed treated as zero") {
    const auto p = P();
    REQUIRE(SwimAnimation::Amplitude(-50.f, p) == SwimAnimation::Amplitude(0.f, p));
}

TEST_CASE("boneCount 0 and negative yield empty vector") {
    auto p = P();
    p.boneCount = 0;
    REQUIRE(SwimAnimation::BoneAngles(0.f, 0.f, 0.f, p).empty());
    p.boneCount = -3;
    REQUIRE(SwimAnimation::BoneAngles(0.f, 0.f, 0.f, p).empty());
}
