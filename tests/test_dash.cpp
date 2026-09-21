#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>

#include "aquarium/Catch.h"
#include "aquarium/Dash.h"
#include "aquarium/Motion.h"

using namespace aquarium;
using Catch::Approx;

namespace { const DashParams kD{}; }

TEST_CASE("a rested dash gives the full burst") {
    DashDrive d;
    d.Press(kD);
    REQUIRE(d.SpeedScale(kD) == Approx(kD.burstScale));
}

TEST_CASE("the burst decays back to normal and stays there") {
    DashDrive d;
    d.Press(kD);
    d.Step(kD.burstDuration * 0.5f, kD);
    const float mid = d.SpeedScale(kD);
    REQUIRE(mid > 1.f);
    REQUIRE(mid < kD.burstScale);
    d.Step(kD.burstDuration, kD);
    REQUIRE(d.SpeedScale(kD) == Approx(1.f));
    d.Step(10.f, kD);
    REQUIRE(d.SpeedScale(kD) == Approx(1.f));
}

TEST_CASE("mashing the key is never REFUSED, only weakened") {
    // 무시는 '고장났다'로 읽힌다. 거절을 표현할 반환값이 Press에 아예 없다.
    DashDrive d;
    float previous = 99.f;
    for (int i = 0; i < 8; ++i) {
        d.Press(kD);
        const float s = d.SpeedScale(kD);
        REQUIRE(s > 1.f);                 // 항상 무언가는 일어난다
        REQUIRE(s <= previous + 1e-4f);   // 점점 약해진다
        previous = s;
        d.Step(0.05f, kD);
    }
    REQUIRE(previous >= 1.f + (kD.burstScale - 1.f) * kD.minChargeFraction - 1e-3f);
}

TEST_CASE("resting restores the full burst") {
    DashDrive d;
    for (int i = 0; i < 6; ++i) { d.Press(kD); d.Step(0.05f, kD); }
    REQUIRE(d.SpeedScale(kD) < kD.burstScale);
    d.Step(kD.rechargeDuration * 2.f, kD);
    d.Press(kD);
    REQUIRE(d.SpeedScale(kD) == Approx(kD.burstScale));
}

TEST_CASE("charge recovers linearly and is clamped to [0,1]") {
    DashDrive d;
    REQUIRE(d.Charge() == Approx(1.f));
    d.Press(kD);
    REQUIRE(d.Charge() == Approx(0.f));
    d.Step(kD.rechargeDuration * 0.5f, kD);
    REQUIRE(d.Charge() == Approx(0.5f));
    d.Step(kD.rechargeDuration * 5.f, kD);
    REQUIRE(d.Charge() == Approx(1.f));
}

TEST_CASE("a zero or negative step changes nothing") {
    DashDrive d;
    d.Press(kD);
    const float before = d.SpeedScale(kD);
    d.Step(0.f, kD);
    d.Step(-1.f, kD);
    REQUIRE(d.SpeedScale(kD) == Approx(before));
}

TEST_CASE("the dash is worth pressing: it beats the catch threshold on its own") {
    // 돌진이 잡기 문턱을 넘기지 못하면 아무 의미가 없다. 이 단언이 두 헤더를 묶는다.
    // CatchParams::catchSpeedFraction = 0.55 -- 평속(1.0배)만으로도 넘지만,
    // 달아나는 놈(도망 2.2배)을 따라잡으려면 돌진이 필요하다.
    REQUIRE(kD.burstScale > 2.2f * 0.5f + 0.55f);
}

TEST_CASE("there is no cooldown that refuses, and no gauge value to display") {
    // 구조로 보장한다: Press에 반환값이 없고, '남은 횟수'를 돌려주는 함수도 없다.
    // Charge()는 0..1 연속값이라 '몇 발 남음'으로 그릴 수 없다.
    DashDrive d;
    d.Press(kD);
    d.Press(kD);
    d.Press(kD);
    REQUIRE(d.Charge() >= 0.f);
    REQUIRE(d.Charge() <= 1.f);
    REQUIRE(d.SpeedScale(kD) > 1.f);
}

// ---------------------------------------------------------------------------
// 돌진과 잡기 문턱을 **한 테스트 안에서** 묶는다. 둘을 따로 두면 M8에서 실제로
// 일어난 일이 다시 일어난다: 문턱은 올려 두고 돌진은 약한 채여서, 방향키로도
// 돌진으로도 아무것도 못 잡는 상태가 초록불로 통과했다(실측: 64초 동안 0마리).
TEST_CASE("a real dash crosses the catch threshold and plain swimming does not") {
    const aquarium::DashParams dp{};
    const aquarium::CatchParams cp{};
    const float maxSpeed = 90.f;      // AquariumGameMode::PlayerMaxSpeed
    const float accel = 140.f;        // AquariumGameMode::PlayerAccel
    const float depth = 220.f;        // 내 물고기의 평면 X
    const float dt = 1.f / 60.f;

    aquarium::MotionParams mp;
    mp.maxSpeed = maxSpeed;
    mp.accel = accel;
    mp.decel = 180.f;
    aquarium::MotionState s;

    // 1) 방향키만: 충분히 오래 달려 최고 속도에 닿는다.
    for (int i = 0; i < 300; ++i) aquarium::StepMotion(s, {1.f, 0.f}, mp, dt);
    const float keysOnly = s.velocity.Length() / depth;
    REQUIRE(keysOnly < aquarium::CatchThreshold(maxSpeed, depth, cp));

    // 2) 같은 자리에서 돌진 한 번. 상한이 올라간 동안 가속이 실제로 따라붙는지를
    //    본다 -- burstDuration이 짧으면 상한만 올라갔다 내려오고 속도는 그대로다.
    // 2) 같은 자리에서 돌진 한 번. 꼭대기 값만 보면 안 된다 -- 상한이 잠깐
    //    올라갔다 내려오는 것만으로도 꼭대기는 문턱을 넘고, 그 상태의 실측이
    //    **64초에 0마리**였다. 아이가 실제로 쓰는 것은 "문턱을 넘은 채로 있는
    //    시간"이고, 그 창이 짧으면 겹치는 순간과 겹치지 않아 아무것도 안 잡힌다.
    aquarium::DashDrive dash;
    dash.Press(dp);
    const float threshold = aquarium::CatchThreshold(maxSpeed, depth, cp);
    float above = 0.f;
    for (int i = 0; i < 180; ++i) {
        dash.Step(dt, dp);
        mp.maxSpeed = maxSpeed * dash.SpeedScale(dp);
        aquarium::StepMotion(s, {1.f, 0.f}, mp, dt);
        if (s.velocity.Length() / depth > threshold) above += dt;
    }
    // 0.4초. 실측으로 정한 값이다: burstDuration 0.35에서는 이 창이 0.4초에 못
    // 미쳤고 돌진을 1.5초마다 눌러도 64초에 0마리였다. 0.9에서는 2~6마리가 된다.
    REQUIRE(above >= 0.4f);
}
