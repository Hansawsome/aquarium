#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "aquarium/Obstacles.h"

using aquarium::Obstacle;
using aquarium::ObstacleParams;
using aquarium::SteerAroundObstacles;
using aquarium::Vec2;

TEST_CASE("no obstacles leaves the direction untouched", "[obstacles]") {
    const ObstacleParams p;
    const Vec2 dir{3.f, 4.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, nullptr, 0, p);
    CHECK(out.x == dir.x);
    CHECK(out.y == dir.y);
}

TEST_CASE("an obstacle dead ahead turns the fish without changing its speed", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{80.f, 0.f}, 30.f}};
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    // This is the whole point of the rule: M3 proved that zeroing the blocked component lets the
    // velocity decelerate through zero, and a zero velocity has no heading, which flipped the
    // visible facing 180 degrees. Magnitude must survive every path through this function.
    CHECK_THAT(out.Length(), Catch::Matchers::WithinAbs(dir.Length(), 1e-3));
    CHECK(out.y != 0.f);                 // actually turned
    CHECK(out.x > 0.f);                  // still going forward, not reversed
}

TEST_CASE("a dead-on obstacle picks the same side every time", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{80.f, 0.f}, 30.f}};
    const Vec2 a = SteerAroundObstacles({0.f, 0.f}, {50.f, 0.f}, o, 1, p);
    const Vec2 b = SteerAroundObstacles({0.f, 0.f}, {50.f, 0.f}, o, 1, p);
    CHECK(a.x == b.x);
    CHECK(a.y == b.y);
    CHECK(a.y > 0.f);                    // documented choice: left of the heading
}

TEST_CASE("an obstacle behind the fish is ignored", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{-80.f, 0.f}, 30.f}};
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    CHECK(out.x == dir.x);
    CHECK(out.y == dir.y);
}

TEST_CASE("an obstacle the ray misses laterally is ignored", "[obstacles]") {
    const ObstacleParams p;                         // margin 20
    const Obstacle o[] = {{{80.f, 100.f}, 30.f}};   // 100 cm off to the side, reach is 50
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    CHECK(out.x == dir.x);
    CHECK(out.y == dir.y);
}

TEST_CASE("an obstacle beyond lookAhead is ignored", "[obstacles]") {
    ObstacleParams p;
    p.lookAhead = 100.f;
    const Obstacle o[] = {{{300.f, 0.f}, 30.f}};
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    CHECK(out.x == dir.x);
    CHECK(out.y == dir.y);
}

TEST_CASE("a fish already inside an obstacle is steered out at the same speed", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{5.f, 0.f}, 40.f}};   // the fish is inside the padded disc
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    CHECK_THAT(out.Length(), Catch::Matchers::WithinAbs(dir.Length(), 1e-3));
    CHECK(out.y != 0.f);                 // pushed sideways rather than teleported
}

TEST_CASE("magnitude is preserved on every path, including many obstacles", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{60.f, 5.f}, 25.f}, {{100.f, -10.f}, 40.f}, {{-50.f, 0.f}, 90.f},
                          {{40.f, 200.f}, 10.f}, {{0.f, 0.f}, 0.f}};
    for (int i = 0; i < 36; ++i) {
        const float a = static_cast<float>(i) * 10.f * 3.14159265f / 180.f;
        const Vec2 dir{37.5f * std::cos(a), 37.5f * std::sin(a)};
        const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 5, p);
        CHECK_THAT(out.Length(), Catch::Matchers::WithinAbs(37.5, 1e-3));
    }
}
