#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "aquarium/Facing.h"

using aquarium::FacingParams;
using aquarium::Steepness;
using aquarium::MaxSwingStepDeg;
using aquarium::MaxTwistStepDeg;

TEST_CASE("a non-positive step permits no rotation at all", "[facing]") {
    const FacingParams p;
    CHECK(MaxSwingStepDeg(p, 0.f) == 0.f);
    CHECK(MaxSwingStepDeg(p, -0.1f) == 0.f);
    CHECK(MaxTwistStepDeg(1.f, p, 0.f) == 0.f);
    CHECK(MaxTwistStepDeg(1.f, p, -0.1f) == 0.f);
}

TEST_CASE("a shallow heading twists at the ordinary turn rate", "[facing]") {
    const FacingParams p;
    CHECK(Steepness(0.f, p) == 0.f);
    CHECK(Steepness(0.5f, p) == 0.f);
    CHECK_THAT(MaxTwistStepDeg(0.f, p, 0.1f), Catch::Matchers::WithinAbs(54.0, 1e-3));
}

TEST_CASE("a vertical heading twists at the fast rate", "[facing]") {
    const FacingParams p;
    CHECK_THAT(Steepness(1.f, p), Catch::Matchers::WithinAbs(1.0, 1e-5));
    // 2880 deg/s spends the unavoidable 180 degree flip in 62 ms, about 4 frames at 60 fps,
    // while the fish is end-on to the camera and the flip cannot be read as a pirouette.
    CHECK_THAT(MaxTwistStepDeg(1.f, p, 0.1f), Catch::Matchers::WithinAbs(288.0, 1e-3));
    CHECK(180.f / p.steepRollRateDegPerSec < 0.07f);
}

TEST_CASE("steepness is monotonic and clamped to 0..1", "[facing]") {
    const FacingParams p;
    float prev = -1.f;
    for (int i = 0; i <= 20; ++i) {
        const float s = static_cast<float>(i) / 20.f;
        const float v = Steepness(s, p);
        CHECK(v >= 0.f);
        CHECK(v <= 1.f);
        CHECK(v >= prev);
        prev = v;
    }
    CHECK(Steepness(1.5f, p) == Steepness(1.f, p));   // clamped past vertical
}

TEST_CASE("a dive and a climb are equally steep", "[facing]") {
    const FacingParams p;
    for (float s : {0.0f, 0.3f, 0.7f, 0.85f, 1.0f}) {
        CHECK(Steepness(s, p) == Steepness(-s, p));
        CHECK(MaxTwistStepDeg(s, p, 0.05f) == MaxTwistStepDeg(-s, p, 0.05f));
    }
}

TEST_CASE("the swing limit is independent of steepness", "[facing]") {
    const FacingParams p;
    CHECK_THAT(MaxSwingStepDeg(p, 0.05f), Catch::Matchers::WithinAbs(27.0, 1e-3));
    // Degenerate params must not divide by zero or invert the rates.
    FacingParams d;
    d.steepBeginSin = 1.f;
    CHECK(Steepness(1.f, d) >= 0.f);
    d.steepRollRateDegPerSec = 10.f;   // slower than upright: clamped up, never down
    CHECK(MaxTwistStepDeg(1.f, d, 0.1f) >= MaxTwistStepDeg(0.f, d, 0.1f));
}
