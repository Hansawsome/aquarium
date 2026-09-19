#include <catch2/catch_test_macros.hpp>
#include "aquarium/Wander.h"

using namespace aquarium;

static const Rect kArea{0.f, 0.f, 100.f, 100.f};

TEST_CASE("same seed yields identical target sequence") {
    WanderBehavior a(42u, kArea), b(42u, kArea);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(a.Target().x == b.Target().x);
        REQUIRE(a.Target().y == b.Target().y);
        a.ForceNewTarget();
        b.ForceNewTarget();
    }
}

TEST_CASE("different seeds yield different targets (no lockstep, F-08)") {
    WanderBehavior a(1u, kArea), b(2u, kArea);
    bool anyDifferent = false;
    for (int i = 0; i < 5; ++i) {
        if (a.Target().x != b.Target().x || a.Target().y != b.Target().y) anyDifferent = true;
        a.ForceNewTarget();
        b.ForceNewTarget();
    }
    REQUIRE(anyDifferent);
}

TEST_CASE("targets stay inside swim area") {
    WanderBehavior w(7u, kArea);
    for (int i = 0; i < 50; ++i) {
        Vec2 t = w.Target();
        REQUIRE(t.x >= kArea.minX); REQUIRE(t.x <= kArea.maxX);
        REQUIRE(t.y >= kArea.minY); REQUIRE(t.y <= kArea.maxY);
        w.ForceNewTarget();
    }
}

TEST_CASE("reaching target picks a new one") {
    WanderBehavior w(7u, kArea);
    const Vec2 first = w.Target();
    w.Update(first, 0.016f);            // standing on the target
    const Vec2 next = w.Target();
    const bool moved = (next.x != first.x) || (next.y != first.y);
    REQUIRE(moved);
}

TEST_CASE("desired direction points toward target") {
    WanderBehavior w(7u, kArea);
    Vec2 pos{0.f, 0.f};
    Vec2 d = w.DesiredDirection(pos);
    Vec2 expect = (w.Target() - pos).Normalized();
    REQUIRE(d.x == expect.x);
    REQUIRE(d.y == expect.y);
}
