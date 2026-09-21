#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "aquarium/Dash.h"

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
