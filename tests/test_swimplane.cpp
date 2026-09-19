#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/SwimPlane.h"

using namespace aquarium;
using Catch::Approx;

static SwimPlane XZPlane() {
    // Unreal: X forward, Y right, Z up. Plane faces the camera: right = +Y, up = +Z.
    return SwimPlane{Vec3{100.f, 0.f, 50.f}, Vec3{0.f, 1.f, 0.f}, Vec3{0.f, 0.f, 1.f}};
}

TEST_CASE("origin maps to plane origin") {
    Vec3 w = XZPlane().ToWorld({0.f, 0.f});
    REQUIRE(w.x == Approx(100.f)); REQUIRE(w.y == Approx(0.f)); REQUIRE(w.z == Approx(50.f));
}

TEST_CASE("x moves along right axis, y along up axis") {
    Vec3 w = XZPlane().ToWorld({3.f, -2.f});
    REQUIRE(w.x == Approx(100.f)); REQUIRE(w.y == Approx(3.f)); REQUIRE(w.z == Approx(48.f));
}

TEST_CASE("forward direction follows 2D velocity in world space") {
    Vec3 f = XZPlane().Forward({0.f, 5.f});
    REQUIRE(f.x == Approx(0.f)); REQUIRE(f.y == Approx(0.f)); REQUIRE(f.z == Approx(1.f));
}

TEST_CASE("forward of zero velocity is zero, not NaN") {
    Vec3 f = XZPlane().Forward({0.f, 0.f});
    REQUIRE(f.x == 0.f); REQUIRE(f.y == 0.f); REQUIRE(f.z == 0.f);
}
