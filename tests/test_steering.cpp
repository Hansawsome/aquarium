#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Steering.h"

using namespace aquarium;
using Catch::Approx;

TEST_CASE("single keys map to screen directions") {
    REQUIRE(SteeringVector({true, false, false, false}).y == Approx(1.f));   // up
    REQUIRE(SteeringVector({false, true, false, false}).y == Approx(-1.f));  // down
    REQUIRE(SteeringVector({false, false, true, false}).x == Approx(-1.f));  // left
    REQUIRE(SteeringVector({false, false, false, true}).x == Approx(1.f));   // right
}

TEST_CASE("opposing keys cancel") {
    Vec2 v = SteeringVector({true, true, true, true});
    REQUIRE(v.x == 0.f);
    REQUIRE(v.y == 0.f);
}

TEST_CASE("diagonal is normalized to unit length") {
    Vec2 v = SteeringVector({true, false, false, true});
    REQUIRE(v.Length() == Approx(1.f));
    REQUIRE(v.x > 0.f);
    REQUIRE(v.y > 0.f);
}

TEST_CASE("no keys means zero vector") {
    Vec2 v = SteeringVector({false, false, false, false});
    REQUIRE(v.Length() == 0.f);
}
