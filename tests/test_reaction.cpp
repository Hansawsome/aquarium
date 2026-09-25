#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "aquarium/Flee.h"
#include "aquarium/Reaction.h"

using namespace aquarium;
using Catch::Approx;

namespace {
const FleeParams kF{};
const PlayerReactionParams kR{};
float Abs(float v) { return v < 0.f ? -v : v; }
}

TEST_CASE("reaction style varies between fish (scenario scene 2, req 2)") {
    int counts[3] = {0, 0, 0};
    for (unsigned s = 1; s <= 300; ++s) {
        counts[static_cast<int>(PickReactionStyle(s))] += 1;
    }
    // 세 가지가 전부 나와야 한다. 하나라도 0이면 "매번 조금씩 다르다"가 거짓이 된다.
    REQUIRE(counts[0] > 40);
    REQUIRE(counts[1] > 40);
    REQUIRE(counts[2] > 40);
    // 같은 시드는 같은 스타일(결정적)
    REQUIRE(PickReactionStyle(77u) == PickReactionStyle(77u));
}

TEST_CASE("every style startles: faster than normal and visibly turning or shaking") {
    for (int i = 0; i < 3; ++i) {
        const StartleShape s = ShapeFor(static_cast<ReactionStyle>(i), kF);
        REQUIRE(s.speedScale > 1.2f);                       // 확실히 빨라진다
        REQUIRE((s.spinDegPerSec != 0.f || s.wobbleDeg > 0.f));  // 눈에 보이는 몸짓이 있다
    }
    // 그리고 셋이 서로 달라야 한다 -- 이름만 셋이고 값이 같으면 아무 의미가 없다.
    const StartleShape a = ShapeFor(ReactionStyle::Dart, kF);
    const StartleShape b = ShapeFor(ReactionStyle::Spin, kF);
    const StartleShape c = ShapeFor(ReactionStyle::Tumble, kF);
    REQUIRE(((a.spinDegPerSec != b.spinDegPerSec) || (a.wobbleDeg != b.wobbleDeg)));
    REQUIRE(((b.spinDegPerSec != c.spinDegPerSec) || (b.wobbleDeg != c.wobbleDeg)));
    REQUIRE(((a.speedScale != c.speedScale) || (a.spinDegPerSec != c.spinDegPerSec)));
}

TEST_CASE("reaction starts instantly, with no ramp-in (scenario scene 2, req 1)") {
    // 0.05초 안에 시작한다는 요구는 60fps에서 세 프레임이다. 램프 인이 있으면
    // 첫 프레임 값이 1에 가깝다. 그래서 dt가 전혀 흐르지 않은 시점을 본다.
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kF);
    REQUIRE(m.State() == BehaviorState::Fleeing);
    REQUIRE(m.SpeedScale(kF) == Approx(kF.fleeSpeedScale));
    REQUIRE(m.FleeDirection().Length() == Approx(1.f));
    // 0.05초 뒤에도 여전히 최대치(줄어들기 시작하지 않는다)
    m.Step(0.05f);
    REQUIRE(m.SpeedScale(kF) == Approx(kF.fleeSpeedScale));
}

TEST_CASE("re-touch mid-flee re-aims instead of being ignored (scenario scene 2, req 4)") {
    // 이 테스트는 M5의 'F-11: 도망 중 재터치 무시'를 **의도적으로 뒤집은 것**이다.
    // 아이는 초당 서너 번 마구 찍고, 무시는 즉각성을 통째로 무너뜨린다.
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kF);
    const Vec2 first = m.FleeDirection();
    m.Step(0.2f);
    m.Touch({20.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kF);   // 반대쪽에서 다시 찍는다
    const Vec2 second = m.FleeDirection();
    REQUIRE(second.x < 0.f);                       // 새 클릭에서 멀어진다
    REQUIRE(first.x > 0.f);                        // 첫 클릭과는 반대 방향이다
    REQUIRE(m.TouchCount() == 2);                  // 두 번 다 셌다 -- 삼킨 클릭이 없다
    REQUIRE(m.State() == BehaviorState::Fleeing);
}

TEST_CASE("every touch is counted, however fast they come") {
    FleeStateMachine m;
    for (int i = 0; i < 25; ++i) {
        m.Touch({static_cast<float>(i), 0.f}, {10.f, 5.f}, {0.f, 0.f}, {0.f, 1.f}, kF);
        m.Step(0.016f);
    }
    REQUIRE(m.TouchCount() == 25);
}

TEST_CASE("player reaction is visible within 0.05 s, whatever the style") {
    // 시나리오 장면 2 요구사항 1은 반응 전체에 걸린다 -- 내 물고기의 재롱도
    // 0.05초 안에 눈에 보여야 한다. 스타일마다 확인한다.
    for (unsigned seed = 1; seed <= 60u; ++seed) {
        PlayerReaction r;
        r.Touch(seed);
        r.Step(0.05f);
        REQUIRE(r.Active());
        REQUIRE(Abs(r.RollOffsetDeg(kR)) > 1.f);
    }
}

TEST_CASE("player reaction has no way to take control away (scenario decision table)") {
    // 이것은 성능이 아니라 **정확성** 요구다. 내 물고기는 도망가지 않고, 조종권을
    // 한 순간도 잃지 않는다. 그래서 이 타입에는 방향·속도·입력을 돌려주는 함수가
    // 하나도 없다. 있는 것은 시각 오프셋뿐이라, 이 반응이 조종을 건드리는 코드를
    // 쓰는 것 자체가 불가능하다.
    PlayerReaction r;
    REQUIRE_FALSE(r.Active());
    r.Touch(5u);
    REQUIRE(r.Active());
    float t = 0.f;
    while (t < kR.duration * 2.f) { r.Step(0.02f); t += 0.02f; }
    REQUIRE_FALSE(r.Active());
    REQUIRE(r.RollOffsetDeg(kR) == Approx(0.f));   // 끝나면 자세가 원래대로 돌아온다
}

TEST_CASE("player reaction bounded: the fish never spins off the screen") {
    for (unsigned seed = 1; seed <= 30u; ++seed) {
        PlayerReaction r;
        r.Touch(seed);
        float worst = 0.f;
        for (int i = 0; i < 200; ++i) {
            r.Step(0.01f);
            const float a = Abs(r.RollOffsetDeg(kR));
            worst = a > worst ? a : worst;
        }
        // 한 바퀴(360도)까지는 재롱이고 그 이상은 고장으로 보인다.
        REQUIRE(worst <= 360.f + 1e-3f);
    }
}

TEST_CASE("player reaction re-touch restarts rather than stacking") {
    PlayerReaction r;
    r.Touch(1u);
    r.Step(kR.duration * 0.9f);
    r.Touch(2u);                     // 아이가 또 찍는다
    REQUIRE(r.Active());
    r.Step(kR.duration * 0.9f);
    REQUIRE(r.Active());             // 새로 시작했으므로 아직 살아 있다
    REQUIRE(r.TouchCount() == 2);
}
