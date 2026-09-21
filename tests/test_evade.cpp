#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "aquarium/Evade.h"

using namespace aquarium;
using Catch::Approx;

namespace {
const EvadeParams kE{};

// 화면 좌표에서의 추격자. 깊이 정규화는 Catch.h가 이미 하므로 여기는 화면 단위만 본다.
Approach Toward(Vec2 from, Vec2 to, float speed) {
    Approach a;
    a.screenPos = from;
    a.screenVel = (to - from).Normalized() * speed;
    return a;
}
}

TEST_CASE("a fish notices a fast approach that is aimed at it") {
    const Approach a = Toward({-0.10f, 0.f}, {0.f, 0.f}, 0.5f);
    REQUIRE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("a fish ignores someone passing by, however close") {
    // 옆으로 스쳐 지나가는 것에까지 반응하면 바다 전체가 계속 파닥거려서
    // '내가 노린 놈이 반응했다'가 읽히지 않는다.
    // 추격자는 바로 아래(0.02)에 있고 **가로로** 지나간다 -- 나를 향한 성분이 0이다.
    Approach a;
    a.screenPos = {0.f, -0.02f};
    a.screenVel = {0.5f, 0.f};
    REQUIRE_FALSE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("a fish ignores an approach that is far away") {
    const Approach a = Toward({-2.f, 0.f}, {0.f, 0.f}, 0.5f);
    REQUIRE_FALSE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("a fish ignores a slow approach: it is not being chased") {
    const Approach a = Toward({-0.10f, 0.f}, {0.f, 0.f}, 0.002f);
    REQUIRE_FALSE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("a fish ignores someone moving away from it") {
    const Approach a = Toward({-0.10f, 0.f}, {-1.f, 0.f}, 0.5f);
    REQUIRE_FALSE(ShouldNotice({0.f, 0.f}, a, kE));
}

TEST_CASE("the dodge is perpendicular to the approach -- that is 'jumps aside'") {
    const Vec2 approachDir{1.f, 0.f};
    const Vec2 d = DodgeDirection(approachDir, /*myVel*/ {0.f, 3.f}, /*seed*/ 1u);
    REQUIRE(d.Length() == Approx(1.f));
    REQUIRE(d.x == Approx(0.f).margin(1e-5f));    // 접근선과 수직
}

TEST_CASE("the dodge keeps the side the fish was already going -- so it can be read") {
    const Vec2 approachDir{1.f, 0.f};
    const Vec2 up = DodgeDirection(approachDir, {0.f, 5.f}, 1u);
    const Vec2 down = DodgeDirection(approachDir, {0.f, -5.f}, 1u);
    REQUIRE(up.y > 0.f);
    REQUIRE(down.y < 0.f);
}

TEST_CASE("a fish with no momentum still dodges, deterministically") {
    const Vec2 a = DodgeDirection({1.f, 0.f}, {0.f, 0.f}, 7u);
    const Vec2 b = DodgeDirection({1.f, 0.f}, {0.f, 0.f}, 7u);
    REQUIRE(a.Length() == Approx(1.f));
    REQUIRE(a.y == Approx(b.y));
    // 시드가 다르면 언젠가는 반대쪽도 나온다.
    bool sawOther = false;
    for (unsigned s = 0; s < 64u; ++s) {
        if (DodgeDirection({1.f, 0.f}, {0.f, 0.f}, s).y * a.y < 0.f) { sawOther = true; break; }
    }
    REQUIRE(sawOther);
}

TEST_CASE("a degenerate approach direction never produces a zero dodge") {
    // 0을 돌려주면 엔진은 '입력 없음'으로 읽고 감속한다 -- 놀라서 멈추는 물고기다.
    const Vec2 d = DodgeDirection({0.f, 0.f}, {0.f, 0.f}, 3u);
    REQUIRE(d.Length() == Approx(1.f));
}

TEST_CASE("the evade burst expires on its own and hands control back") {
    EvadeBehavior e;
    e.Notice({1.f, 0.f}, {0.f, 4.f}, 11u, kE);
    REQUIRE(e.Active());
    REQUIRE(e.SpeedScale(kE) > 1.f);
    e.Step(kE.duration * 0.5f);
    REQUIRE(e.Active());
    e.Step(kE.duration * 0.6f);
    REQUIRE_FALSE(e.Active());
    REQUIRE(e.SpeedScale(kE) == Approx(1.f));
}

TEST_CASE("noticing again while dodging re-aims and refills -- never 'ignored'") {
    // F-11에서 배운 것: 무시는 아이에게 '고장났다'로 읽힌다. 회피도 같다.
    EvadeBehavior e;
    e.Notice({1.f, 0.f}, {0.f, 4.f}, 11u, kE);
    e.Step(kE.duration * 0.9f);
    const Vec2 first = e.Direction();
    e.Notice({0.f, 1.f}, {4.f, 0.f}, 11u, kE);
    REQUIRE(e.Active());
    REQUIRE(e.NoticeCount() == 2);
    REQUIRE_FALSE(e.Direction().x == Approx(first.x));
    e.Step(kE.duration * 0.9f);
    REQUIRE(e.Active());          // 타이머가 새로 채워졌다
}

TEST_CASE("the dodge is faster than normal swimming but slower than a full startle") {
    // 회피가 놀람보다 세면 클릭이 의미를 잃는다(클릭은 몰이 도구여야 한다).
    REQUIRE(kE.dodgeSpeedScale > 1.f);
    REQUIRE(kE.dodgeSpeedScale < 2.2f);   // FleeParams::fleeSpeedScale
}
