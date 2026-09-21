#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "aquarium/Bubbles.h"

using namespace aquarium;
using Catch::Approx;

namespace {
const BubbleParams kB{};
const float kTop = 150.f;   // 유영 영역 반높이. 엔진이 Rect에서 파생해 넘긴다
}

TEST_CASE("a bubble rises") {
    Bubble b = MakeBubble({10.f, -100.f}, 400.f, 1u, kB);
    const float y0 = b.position.y;
    StepBubble(b, 0.5f, kB);
    REQUIRE(b.position.y > y0);
}

TEST_CASE("bubbles never disappear below the top of the screen (scene 2, req 3)") {
    // 이것이 '시선의 약속'이다. 어떤 시드로도, 얼마나 오래 흘러도, 화면 위를
    // 넘기 전에는 절대 사라지지 않는다.
    for (unsigned s = 1; s <= 64; ++s) {
        Bubble b = MakeBubble({0.f, -kTop}, 400.f, s, kB);
        for (int i = 0; i < 600; ++i) {          // 10초
            StepBubble(b, 1.f / 60.f, kB);
            if (BubbleIsGone(b, kTop)) {
                REQUIRE(b.position.y >= kTop);   // 넘었을 때만 사라졌다
                break;
            }
            REQUIRE(b.position.y < kTop);
        }
    }
}

TEST_CASE("a bubble does eventually leave the top (it is not immortal)") {
    Bubble b = MakeBubble({0.f, -kTop}, 400.f, 9u, kB);
    int steps = 0;
    while (!BubbleIsGone(b, kTop) && steps < 3000) { StepBubble(b, 1.f / 60.f, kB); ++steps; }
    REQUIRE(BubbleIsGone(b, kTop));
    // 그리고 너무 오래 걸리면 아이가 지겨워한다: 2*kTop 높이를 20초 안에 넘는다.
    REQUIRE(steps < 20 * 60);
}

TEST_CASE("bubbles sway but do not drift away sideways") {
    Bubble b = MakeBubble({0.f, 0.f}, 400.f, 4u, kB);
    float worst = 0.f;
    for (int i = 0; i < 600; ++i) {
        StepBubble(b, 1.f / 60.f, kB);
        const float dx = b.position.x < 0.f ? -b.position.x : b.position.x;
        worst = dx > worst ? dx : worst;
    }
    // 좌우 흔들림은 진폭 안에 갇혀 있어야 한다. 누적 표류가 있으면 기포가
    // 화면 옆으로 빠져 '위로 사라진다'는 약속이 깨진다.
    REQUIRE(worst <= kB.swayAmplitude * 1.2f);
}

TEST_CASE("bubble speed and size vary between bubbles") {
    int distinctSpeed = 0, distinctRadius = 0;
    float prevS = -1.f, prevR = -1.f;
    for (unsigned s = 1; s <= 24; ++s) {
        const Bubble b = MakeBubble({0.f, 0.f}, 400.f, s, kB);
        if (b.riseSpeed != prevS) ++distinctSpeed;
        if (b.radius != prevR) ++distinctRadius;
        prevS = b.riseSpeed; prevR = b.radius;
        REQUIRE(b.riseSpeed > 0.f);
        REQUIRE(b.radius > 0.f);
    }
    REQUIRE(distinctSpeed >= 20);
    REQUIRE(distinctRadius >= 20);
}

TEST_CASE("MakeBubble is deterministic") {
    const Bubble a = MakeBubble({3.f, 4.f}, 500.f, 42u, kB);
    const Bubble b = MakeBubble({3.f, 4.f}, 500.f, 42u, kB);
    REQUIRE(a.riseSpeed == Approx(b.riseSpeed));
    REQUIRE(a.radius == Approx(b.radius));
    REQUIRE(a.swayPhase == Approx(b.swayPhase));
}

TEST_CASE("each bubble is judged by its own top, not one shared height") {
    // 원근 때문에 깊은 평면일수록 화면 위 끝이 높다. 하나의 높이를 공유하면 깊은
    // 곳의 기포가 화면 한가운데에서 사라지고, 아이는 그것을 알아챈다.
    Bubble shallow = MakeBubble({0.f, 0.f}, 220.f, 2u, kB);
    Bubble deep = MakeBubble({0.f, 0.f}, 700.f, 2u, kB);
    shallow.topY = 100.f;
    deep.topY = 900.f;                       // 같은 화면 비율, 3배 이상 깊은 평면
    for (int i = 0; i < 300; ++i) { StepBubble(shallow, 1.f / 60.f, kB); StepBubble(deep, 1.f / 60.f, kB); }
    REQUIRE(BubbleIsGone(shallow));          // 얕은 쪽은 이미 나갔고
    REQUIRE_FALSE(BubbleIsGone(deep));       // 깊은 쪽은 아직 화면 안이다
    // 그리고 결국은 깊은 쪽도 자기 높이를 넘는다.
    for (int i = 0; i < 1800; ++i) { StepBubble(deep, 1.f / 60.f, kB); }
    REQUIRE(BubbleIsGone(deep));
}
