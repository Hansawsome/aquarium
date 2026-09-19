#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Flee.h"

using namespace aquarium;
using Catch::Approx;

static const FleeParams kP{};   // flee 0.8s, recover 1.2s defaults

TEST_CASE("flee direction points away from touch") {
    Vec2 d = ComputeFleeDirection({10.f, 50.f}, {20.f, 50.f}, {0.f, 0.f}, {0.f, 1.f});
    REQUIRE(d.x == Approx(1.f));
    REQUIRE(d.y == Approx(0.f));
}

TEST_CASE("dead-center touch falls back to current heading") {
    Vec2 d = ComputeFleeDirection({20.f, 50.f}, {20.f, 50.f}, {0.f, -3.f}, {0.f, 1.f});
    REQUIRE(d.y == Approx(-1.f));
}

TEST_CASE("dead-center touch on stationary fish uses fallback direction") {
    Vec2 d = ComputeFleeDirection({20.f, 50.f}, {20.f, 50.f}, {0.f, 0.f}, {0.f, 1.f});
    REQUIRE(d.y == Approx(1.f));
}

TEST_CASE("state machine walks flee -> recover -> normal on time boundaries") {
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);
    REQUIRE(m.State() == BehaviorState::Fleeing);
    m.Step(0.8f);
    REQUIRE(m.State() == BehaviorState::Recovering);
    m.Step(1.2f);
    REQUIRE(m.State() == BehaviorState::Normal);
}

TEST_CASE("touch during flee is ignored; touch during recover restarts flee") {
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);
    m.Step(0.4f);
    m.Touch({0.f, 0.f}, {-10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);   // ignored
    REQUIRE(m.FleeDirection().x == Approx(1.f));                      // unchanged
    m.Step(0.4f);                                                     // total 0.8 -> Recovering
    REQUIRE(m.State() == BehaviorState::Recovering);
    m.Touch({20.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);   // restart
    REQUIRE(m.State() == BehaviorState::Fleeing);
    REQUIRE(m.FleeDirection().x == Approx(-1.f));                     // new direction
}

TEST_CASE("player input suppressed while fleeing, restored from recovery (F-12)") {
    FleeStateMachine m;
    const Vec2 player{0.f, 1.f};
    REQUIRE(m.EffectiveInput(player).y == Approx(1.f));               // Normal
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);
    REQUIRE(m.EffectiveInput(player).x == Approx(1.f));               // flee dir wins
    REQUIRE(m.EffectiveInput(player).y == Approx(0.f));
    m.Step(0.8f);                                                     // Recovering
    REQUIRE(m.EffectiveInput(player).y == Approx(1.f));               // player again
}
