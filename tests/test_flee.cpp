#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Flee.h"
#include "aquarium/SwimPlane.h"   // Vec3

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

TEST_CASE("re-touch mid-flee re-aims (M7: F-11's ignore was removed on purpose)") {
    // M5까지는 이 테스트가 '무시된다'를 단언했다. 시나리오(장면 2 요구사항 4)가
    // 연타를 기본 사용법으로 못 박으면서 사양이 뒤집혔다. 기록을 남기려고 지우지
    // 않고 뒤집어 둔다.
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);
    m.Step(0.4f);
    m.Touch({20.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);   // 이제는 다시 겨눈다
    REQUIRE(m.FleeDirection().x == Approx(-1.f));                     // 새 클릭에서 멀어진다
    REQUIRE(m.TouchCount() == 2);                                     // 삼킨 클릭이 없다
    m.Step(0.8f);                                                     // 타이머가 새로 찼다 -> Recovering
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

namespace {
// A fish on the plane X = depth, centred at (worldY, worldZ) = (cy, cz).
aquarium::ClickTarget Target(float depth, float cy, float cz, float hw = 12.f, float hh = 5.f) {
    aquarium::ClickTarget t;
    t.depth = depth;
    t.center = {cy, cz};
    t.halfWidth = hw;
    t.halfHeight = hh;
    return t;
}
// A ray from the camera position straight along +X at height z.
aquarium::Vec3 Along(float z) { return {0.f, 0.f, z}; }
const aquarium::Vec3 kForward{1.f, 0.f, 0.f};
} // namespace

TEST_CASE("empty water hits nothing (F-09)") {
    const aquarium::ClickTarget t[] = {Target(400.f, 100.f, 0.f)};
    REQUIRE(aquarium::PickFrontmostHit(Along(0.f), kForward, t, 1) == -1);
}

TEST_CASE("no targets hits nothing (F-09)") {
    REQUIRE(aquarium::PickFrontmostHit(Along(0.f), kForward, nullptr, 0) == -1);
}

TEST_CASE("a ray through one fish hits it (F-09)") {
    const aquarium::ClickTarget t[] = {Target(400.f, 0.f, 0.f)};
    REQUIRE(aquarium::PickFrontmostHit(Along(0.f), kForward, t, 1) == 0);
}

TEST_CASE("three overlapping fish: only the frontmost is returned (F-09)") {
    // All three project onto the same screen point; only the smallest depth may win.
    const aquarium::ClickTarget t[] = {Target(700.f, 0.f, 0.f), Target(330.f, 0.f, 0.f),
                                       Target(500.f, 0.f, 0.f)};
    REQUIRE(aquarium::PickFrontmostHit(Along(0.f), kForward, t, 3) == 1);
}

TEST_CASE("targets behind the ray origin are ignored (F-09)") {
    const aquarium::ClickTarget t[] = {Target(-100.f, 0.f, 0.f)};
    REQUIRE(aquarium::PickFrontmostHit(Along(0.f), kForward, t, 1) == -1);
}

TEST_CASE("a ray that never reaches the planes hits nothing (F-09)") {
    // Straight up: dir.x is 0, so no plane at a fixed X is ever crossed.
    const aquarium::ClickTarget t[] = {Target(400.f, 0.f, 0.f)};
    REQUIRE(aquarium::PickFrontmostHit(Along(0.f), {0.f, 0.f, 1.f}, t, 1) == -1);
}

TEST_CASE("the hit shape is an ellipse, not a circle (F-09)") {
    // halfWidth 12, halfHeight 5. A point 8 cm above centre is INSIDE a 12 cm circle but
    // OUTSIDE the real fish, which is only 5 cm tall. A circle approximation would hit here.
    const aquarium::ClickTarget t[] = {Target(400.f, 0.f, 0.f, 12.f, 5.f)};
    REQUIRE(aquarium::PickFrontmostHit(Along(8.f), kForward, t, 1) == -1);
    REQUIRE(aquarium::PickFrontmostHit(Along(4.f), kForward, t, 1) == 0);    // inside
    REQUIRE(aquarium::PickFrontmostHit({0.f, 11.f, 0.f}, kForward, t, 1) == 0);  // wide, inside
}

TEST_CASE("an angled ray lands where the geometry says it should (F-09)") {
    // dir = (1, 0, 0.1) normalized-ish: at depth 400 the ray is 40 cm above its start.
    const aquarium::ClickTarget t[] = {Target(400.f, 0.f, 40.f)};
    REQUIRE(aquarium::PickFrontmostHit(Along(0.f), {1.f, 0.f, 0.1f}, t, 1) == 0);
    const aquarium::ClickTarget miss[] = {Target(400.f, 0.f, 0.f)};
    REQUIRE(aquarium::PickFrontmostHit(Along(0.f), {1.f, 0.f, 0.1f}, miss, 1) == -1);
}
