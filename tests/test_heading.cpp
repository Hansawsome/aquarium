#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Heading.h"

using namespace aquarium;
using Catch::Approx;

TEST_CASE("heading of cardinal directions") {
    REQUIRE(HeadingDeg({1.f, 0.f}) == Approx(0.f));
    REQUIRE(HeadingDeg({0.f, 1.f}) == Approx(90.f));
    REQUIRE(HeadingDeg({-1.f, 0.f}) == Approx(180.f));
    REQUIRE(HeadingDeg({0.f, -1.f}) == Approx(-90.f));
}

TEST_CASE("heading of zero vector is 0") {
    REQUIRE(HeadingDeg({0.f, 0.f}) == 0.f);
}

TEST_CASE("turn rate takes the shortest signed delta across the wrap") {
    REQUIRE(TurnRateDegPerSec(170.f, -170.f, 0.1f) == Approx(200.f));
    REQUIRE(TurnRateDegPerSec(-170.f, 170.f, 0.1f) == Approx(-200.f));
    REQUIRE(TurnRateDegPerSec(0.f, 30.f, 0.5f) == Approx(60.f));
}

TEST_CASE("turn rate at the +-180 boundary resolves to +180") {
    REQUIRE(TurnRateDegPerSec(0.f, 180.f, 1.f) == Approx(180.f));
    REQUIRE(TurnRateDegPerSec(0.f, -180.f, 1.f) == Approx(180.f));
}

TEST_CASE("turn rate is 0 for dt <= 0") {
    REQUIRE(TurnRateDegPerSec(10.f, 50.f, 0.f) == 0.f);
    REQUIRE(TurnRateDegPerSec(10.f, 50.f, -1.f) == 0.f);
}
