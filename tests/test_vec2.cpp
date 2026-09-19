#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Vec2.h"

using aquarium::Vec2;
using Catch::Approx;

TEST_CASE("length of 3-4 vector is 5") {
    REQUIRE(Vec2{3.f, 4.f}.Length() == Approx(5.f));
}

TEST_CASE("Normalized keeps direction, unit length") {
    Vec2 n = Vec2{10.f, 0.f}.Normalized();
    REQUIRE(n.x == Approx(1.f));
    REQUIRE(n.y == Approx(0.f));
}

TEST_CASE("Normalized of zero vector is zero, not NaN") {
    Vec2 n = Vec2{0.f, 0.f}.Normalized();
    REQUIRE(n.x == 0.f);
    REQUIRE(n.y == 0.f);
}

TEST_CASE("arithmetic operators") {
    Vec2 v = Vec2{1.f, 2.f} + Vec2{3.f, 4.f} * 2.f;
    REQUIRE(v.x == Approx(7.f));
    REQUIRE(v.y == Approx(10.f));
}
