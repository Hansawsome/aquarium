#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "aquarium/Impact.h"

using namespace aquarium;
using Catch::Approx;

namespace { const ImpactParams kI{}; }

TEST_CASE("the shake starts immediately at full amplitude") {
    // 램프 인이 있으면 아이 눈에 늦게 시작한 것으로 보인다(장면 2 요구사항 1과 같은 이유).
    ImpactShake s;
    s.Hit(kI);
    const ShakeOffset o = s.Offset(kI);
    REQUIRE(std::abs(o.y) + std::abs(o.z) > kI.amplitudeCm * 0.5f);
}

TEST_CASE("the shake is SHORT and ends exactly at zero") {
    ImpactShake s;
    s.Hit(kI);
    s.Step(kI.duration * 0.999f);
    REQUIRE(s.Active());
    s.Step(kI.duration);
    REQUIRE_FALSE(s.Active());
    const ShakeOffset o = s.Offset(kI);
    REQUIRE(o.y == Approx(0.f));
    REQUIRE(o.z == Approx(0.f));
}

TEST_CASE("the shake decays: later is smaller than earlier") {
    ImpactShake a, b;
    a.Hit(kI); b.Hit(kI);
    b.Step(kI.duration * 0.7f);
    const float ea = std::abs(a.Offset(kI).y) + std::abs(a.Offset(kI).z);
    const float eb = std::abs(b.Offset(kI).y) + std::abs(b.Offset(kI).z);
    REQUIRE(eb < ea);
}

TEST_CASE("the shake never leaves a few centimetres -- it is a bump, not an earthquake") {
    ImpactShake s;
    s.Hit(kI);
    for (int i = 0; i < 200; ++i) {
        const ShakeOffset o = s.Offset(kI);
        REQUIRE(std::abs(o.y) <= kI.amplitudeCm + 1e-3f);
        REQUIRE(std::abs(o.z) <= kI.amplitudeCm + 1e-3f);
        s.Step(kI.duration / 100.f);
    }
}

TEST_CASE("the two axes are not the same wave") {
    // 두 축이 같은 위상이면 대각선으로만 흔들려 '툭 튕겼다'가 아니라 '미끄러졌다'가 된다.
    ImpactShake s;
    s.Hit(kI);
    s.Step(kI.duration * 0.15f);
    const ShakeOffset o = s.Offset(kI);
    REQUIRE_FALSE(o.y == Approx(o.z));
}

TEST_CASE("hitting again while shaking restarts, it does not stack") {
    // 겹쳐 쌓이면 연타할 때 화면이 아이를 멀미하게 만든다.
    ImpactShake s;
    s.Hit(kI);
    s.Step(kI.duration * 0.8f);
    s.Hit(kI);
    s.Step(kI.duration * 0.5f);
    REQUIRE(s.Active());
    const ShakeOffset o = s.Offset(kI);
    REQUIRE(std::abs(o.y) <= kI.amplitudeCm + 1e-3f);
}

TEST_CASE("the water displacement weight follows the same short life") {
    ImpactShake s;
    s.Hit(kI);
    REQUIRE(s.DisplacementWeight(kI) > 0.5f);
    s.Step(kI.duration * 2.f);
    REQUIRE(s.DisplacementWeight(kI) == Approx(0.f));
}

TEST_CASE("displacement decays monotonically -- no flicker") {
    // 왜곡이 사인처럼 깜빡이면 화면이 지글거린다. 흔들림만 진동하고 왜곡은 내려가기만 한다.
    ImpactShake s;
    s.Hit(kI);
    float previous = s.DisplacementWeight(kI);
    for (int i = 0; i < 30; ++i) {
        s.Step(kI.duration / 30.f);
        const float w = s.DisplacementWeight(kI);
        REQUIRE(w <= previous + 1e-5f);
        previous = w;
    }
}
