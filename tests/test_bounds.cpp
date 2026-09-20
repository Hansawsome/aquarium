#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Bounds.h"

using namespace aquarium;
using Catch::Approx;

static const Rect kArea{0.f, 0.f, 100.f, 100.f};

TEST_CASE("direction unchanged in open water") {
    Vec2 d = AvoidBoundary({50.f, 50.f}, {1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == Approx(1.f));
}

TEST_CASE("outward component removed near each edge") {
    REQUIRE(AvoidBoundary({95.f, 50.f}, {1.f, 0.f}, kArea, 10.f).x == 0.f);   // right
    REQUIRE(AvoidBoundary({5.f, 50.f}, {-1.f, 0.f}, kArea, 10.f).x == 0.f);   // left
    REQUIRE(AvoidBoundary({50.f, 95.f}, {0.f, 1.f}, kArea, 10.f).y == 0.f);   // top
    REQUIRE(AvoidBoundary({50.f, 5.f}, {0.f, -1.f}, kArea, 10.f).y == 0.f);   // bottom
}

TEST_CASE("inward movement near edge is not blocked") {
    Vec2 d = AvoidBoundary({95.f, 50.f}, {-1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == Approx(-1.f));
}

TEST_CASE("corner blocks both outward axes") {
    Vec2 d = AvoidBoundary({95.f, 95.f}, Vec2{1.f, 1.f}.Normalized(), kArea, 10.f);
    REQUIRE(d.x == 0.f);
    REQUIRE(d.y == 0.f);
}

TEST_CASE("clamp forces position inside area") {
    Vec2 p = ClampToArea({150.f, -20.f}, kArea);
    REQUIRE(p.x == Approx(100.f));
    REQUIRE(p.y == Approx(0.f));
}

TEST_CASE("steer along boundary keeps direction in open water") {
    Vec2 d = SteerAlongBoundary({50.f, 50.f}, Vec2{1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == Approx(1.f)); REQUIRE(d.y == Approx(0.f));
}
TEST_CASE("steer along boundary slides along a wall at full magnitude") {
    const Vec2 In = Vec2{1.f, 1.f}.Normalized();
    Vec2 d = SteerAlongBoundary({95.f, 50.f}, In, kArea, 10.f);
    REQUIRE(d.x == 0.f); REQUIRE(d.y == Approx(1.f)); REQUIRE(d.Length() == Approx(1.f));
}
TEST_CASE("steer along boundary turns inward in a corner") {
    Vec2 d = SteerAlongBoundary({95.f, 95.f}, Vec2{1.f, 1.f}.Normalized(), kArea, 10.f);
    REQUIRE(d.x < 0.f); REQUIRE(d.y < 0.f); REQUIRE(d.Length() == Approx(1.f));
}
TEST_CASE("steer along boundary does not block inward movement") {
    Vec2 d = SteerAlongBoundary({95.f, 50.f}, Vec2{-1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == Approx(-1.f));
}
TEST_CASE("steer along boundary turns aside when heading straight at a wall") {
    // The common case: a fish swimming dead-on into a wall. Dropping the outward component
    // would leave a zero direction (the stall this function exists to remove), so the
    // direction turns along the wall toward the area centre at full magnitude.
    Vec2 d = SteerAlongBoundary({95.f, 30.f}, Vec2{1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == 0.f);
    REQUIRE(d.y == Approx(1.f));          // centre is at y = 50, so it heads up the wall
    REQUIRE(d.Length() == Approx(1.f));
    Vec2 e = SteerAlongBoundary({95.f, 70.f}, Vec2{1.f, 0.f}, kArea, 10.f);
    REQUIRE(e.y == Approx(-1.f));         // below the centre in y: heads down the wall
}

TEST_CASE("steer along boundary passes a zero direction through") {
    Vec2 d = SteerAlongBoundary({95.f, 95.f}, Vec2{0.f, 0.f}, kArea, 10.f);
    REQUIRE(d.Length() == 0.f);
}
