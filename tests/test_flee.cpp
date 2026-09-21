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

TEST_CASE("flee direction is never a zero vector (F-10)") {
    // Dead-centre touch, stationary fish, AND a degenerate fallback: this happens when a fish
    // sits exactly at its plane centre (so "toward screen centre" is also zero) and the child
    // clicks it. Returning {0,0} here would make StepMotion decelerate -- the fish would be
    // startled into stopping.
    const Vec2 d = ComputeFleeDirection({20.f, 50.f}, {20.f, 50.f}, {0.f, 0.f}, {0.f, 0.f});
    REQUIRE(d.Length() == Approx(1.f));
}

TEST_CASE("speed scale is 1 while normal (F-10)") {
    FleeStateMachine m;
    REQUIRE(m.SpeedScale(kP) == Approx(1.f));
}

TEST_CASE("speed scale bursts while fleeing (F-10)") {
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);
    REQUIRE(m.SpeedScale(kP) == Approx(kP.fleeSpeedScale));
    m.Step(0.4f);
    REQUIRE(m.SpeedScale(kP) == Approx(kP.fleeSpeedScale));   // flat for the whole flee
}

TEST_CASE("speed scale ramps back to 1 across recovery (F-10)") {
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);
    m.Step(0.8f);                                   // -> Recovering, full 1.2 s left
    REQUIRE(m.State() == BehaviorState::Recovering);
    REQUIRE(m.SpeedScale(kP) == Approx(kP.fleeSpeedScale));
    m.Step(0.6f);                                   // half way through recovery
    const float mid = m.SpeedScale(kP);
    REQUIRE(mid == Approx(1.f + (kP.fleeSpeedScale - 1.f) * 0.5f).margin(1e-3f));
    m.Step(0.6f);                                   // -> Normal
    REQUIRE(m.State() == BehaviorState::Normal);
    REQUIRE(m.SpeedScale(kP) == Approx(1.f));
}

TEST_CASE("one huge step lands in Normal, not stuck in Recovering (F-11)") {
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);
    m.Step(5.f);                                    // longer than flee + recover together
    REQUIRE(m.State() == BehaviorState::Normal);
    REQUIRE(m.SpeedScale(kP) == Approx(1.f));
}
