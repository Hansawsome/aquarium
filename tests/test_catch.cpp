#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

#include "aquarium/Catch.h"

using namespace aquarium;
using Catch::Approx;

namespace {
const CatchParams kC{};

// 배경 물고기 한 마리. 기본값은 실제 장면에 가깝게: 깊이 400, 길이 25cm급.
RamTarget Fish(float depth, Vec2 centre, Vec2 vel = {0.f, 0.f}) {
    RamTarget t;
    t.depth = depth;
    t.center = centre;
    t.velocity = vel;
    t.halfWidth = 12.5f;
    t.halfHeight = 4.f;
    return t;
}

// 카메라는 원점, 평면들은 +X 앞에 있다(실제 배치와 같은 부호).
const Vec3 kCam{0.f, 0.f, 0.f};

// 방향키만으로 낼 수 있는 속도(= 최대 속도 그대로)와, **돌진 중에 실제로 도달하는**
// 속도. 후자는 예측이 아니라 실측이다: DashParams(burstScale 2.4, burstDuration 0.9)와
// 플레이어의 가속 140 cm/s^2에서 속도 상한(216까지 올랐다가 0.9초에 걸쳐 90으로
// 내려온다)과 가속선이 만나는 점이 약 1.7배다. 실제 RHI 실행에서 관측된 접근 속도도
// 방향키만일 때 최대 0.44 화면단위/초, 돌진을 섞으면 0.59까지 올라갔다.
const float kKeysOnlySpeed = 90.f;
const float kDashSpeed = 90.f * 1.7f;
}

TEST_CASE("a fast nose on the target catches it") {
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};              // 화면에서 정확히 겹친다
    me.velocity = {kDashSpeed, 0.f};   // 돌진으로만 낼 수 있는 속도
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Catch);
    REQUIRE(r.targetIndex == 0);
    REQUIRE(r.closingSpeed > 0.f);
}

TEST_CASE("arrow keys alone, at full speed, only bump -- the dash is the catch") {
    // **사양 변경(M8 난이도 재조정).** 예전에는 방향키만으로 똑바로 달리면 잡혔고,
    // 그래서 아무렇게나 헤엄쳐도 1분에 22마리가 찍혔다(실측). 그러면 돌진 키가 할 일이
    // 없어지고 '숙련의 게임'이라는 축이 통째로 죽는다. 이제 최고 속도로 정면을
    // 들이받아도 Bump다 -- 잡으려면 반드시 돌진해야 한다.
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {kKeysOnlySpeed, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Bump);
    REQUIRE(r.targetIndex == 0);
}

TEST_CASE("drifting into a fish does NOT catch it -- it only bumps") {
    // 관성으로 슬금슬금 겹쳐지는 것은 잡기가 아니다. 이 한 줄이 '어려움'의 전부다.
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {6.f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Bump);
    REQUIRE(r.targetIndex == 0);
}

TEST_CASE("chasing a fish at the same speed never catches it") {
    // 시나리오의 그림 그대로: 달아나는 놈을 같은 속도로 따라가면 못 잡는다.
    // 깊이가 달라 화면 속도가 다르므로, 두 속도는 **화면에서** 같아지도록 만든다.
    const float mineDepth = 220.f, itsDepth = 400.f;
    const float myScreenSpeed = kDashSpeed / mineDepth;
    std::vector<RamTarget> targets{Fish(itsDepth, {0.f, 0.f}, {myScreenSpeed * itsDepth, 0.f})};
    Rammer me;
    me.depth = mineDepth;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    // 돌진 중이어도 상대가 화면에서 같은 속도로 달아나면 접근 속도는 0이다.
    me.velocity = {kDashSpeed, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Bump);
    REQUIRE(r.closingSpeed == Approx(0.f).margin(1e-3f));
}

TEST_CASE("catching a fleeing fish needs to be FASTER than it") {
    const float mineDepth = 220.f, itsDepth = 400.f;
    const float itsScreenSpeed = 40.f / itsDepth;
    std::vector<RamTarget> targets{Fish(itsDepth, {0.f, 0.f}, {40.f, 0.f})};
    Rammer me;
    me.depth = mineDepth;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    // 화면 기준으로 상대보다 충분히 빠르게: 문턱 + 상대의 화면 속도.
    const float need = CatchThreshold(90.f, mineDepth, kC) + itsScreenSpeed;
    me.velocity = {need * mineDepth * 1.05f, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Catch);
}

TEST_CASE("no overlap is a plain miss, whatever the speed") {
    std::vector<RamTarget> targets{Fish(400.f, {900.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {kDashSpeed, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Miss);
    REQUIRE(r.targetIndex == -1);
}

TEST_CASE("depth is normalized: two fish that look identical behave identically") {
    // 같은 화면 위치·같은 화면 크기인데 깊이만 다른 두 마리. 하나가 더 잡기 쉬우면
    // 아이는 이유를 알 수 없다 -- 화면에서는 완전히 같아 보이기 때문이다.
    RamTarget near_ = Fish(400.f, {40.f, 0.f});
    RamTarget far_ = near_;
    far_.depth = 800.f;
    far_.center = {80.f, 0.f};          // (pos/depth)가 같도록
    far_.halfWidth = near_.halfWidth * 2.f;
    far_.halfHeight = near_.halfHeight * 2.f;
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {22.f, 0.f};
    me.velocity = {kDashSpeed, 0.f};
    std::vector<RamTarget> a{near_};
    std::vector<RamTarget> b{far_};
    const RamResult ra = EvaluateRam(kCam, me, a.data(), a.size(), kC);
    const RamResult rb = EvaluateRam(kCam, me, b.data(), b.size(), kC);
    // **둘 다 실제로 맞았다**는 것까지 단언한다. 이것이 없으면 깊이 정규화를
    // 지우는 변이가 '둘 다 Miss'로 조용히 통과한다(계획서 원안의 결함).
    REQUIRE(ra.outcome == RamOutcome::Catch);
    REQUIRE(ra.outcome == rb.outcome);
    REQUIRE(ra.closingSpeed == Approx(rb.closingSpeed));
}

TEST_CASE("the frontmost overlapping fish is the one that gets hit") {
    // 두 마리가 화면에서 겹쳐 있으면 앞엣놈이다. 클릭(PickFrontmostHit)과 같은 규칙이라
    // 아이가 배울 규칙이 하나뿐이다.
    std::vector<RamTarget> targets{Fish(700.f, {0.f, 0.f}), Fish(380.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {kDashSpeed, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.targetIndex == 1);
}

TEST_CASE("an already stamped fish can still be bumped but reports as stamped") {
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    targets[0].alreadyStamped = true;
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    me.velocity = {kDashSpeed, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    // 잡히는 것 자체는 막지 않는다. 숫자를 두 번 올리지 않는 일은 StampBook이 한다.
    REQUIRE(r.outcome == RamOutcome::Catch);
    REQUIRE(r.targetIndex == 0);
}

TEST_CASE("the nose sits ahead of the body, not at its centre") {
    // 머리로 받아야 '들이받았다'이다. 꼬리로 스친 것이 잡기가 되면 아이는 무엇을 한 건지 모른다.
    const Vec2 heading{1.f, 0.f};
    const Vec2 nose = NosePoint({0.f, 0.f}, heading, /*halfLength*/ 17.f, kC);
    REQUIRE(nose.x > 0.f);
    REQUIRE(nose.x <= 17.f + 1e-3f);
    // 방향이 0이면(정지) 몸 중심 그대로. 0으로 나누지 않는다.
    const Vec2 still = NosePoint({5.f, 6.f}, {0.f, 0.f}, 17.f, kC);
    REQUIRE(still.x == Approx(5.f));
    REQUIRE(still.y == Approx(6.f));
}

TEST_CASE("the catch threshold is DERIVED from the player's own max speed") {
    // 문턱을 리터럴로 적으면 최대 속도를 조정할 때 난이도가 조용히 어긋난다(규약 8).
    const float t1 = CatchThreshold(90.f, 220.f, kC);
    const float t2 = CatchThreshold(180.f, 220.f, kC);
    REQUIRE(t2 == Approx(t1 * 2.f));
    REQUIRE(t1 > 0.f);
}

TEST_CASE("nothing to ram is a miss, not a crash") {
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.velocity = {kDashSpeed, 0.f};
    const RamResult r = EvaluateRam(kCam, me, nullptr, 0, kC);
    REQUIRE(r.outcome == RamOutcome::Miss);
    REQUIRE(r.targetIndex == -1);
}

TEST_CASE("a target behind the camera is never hit") {
    std::vector<RamTarget> targets{Fish(-100.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.velocity = {kDashSpeed, 0.f};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Miss);
}

TEST_CASE("a zero-size target is skipped") {
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    targets[0].halfWidth = 0.f;
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.velocity = {kDashSpeed, 0.f};
    REQUIRE(EvaluateRam(kCam, me, targets.data(), targets.size(), kC).outcome == RamOutcome::Miss);
}

TEST_CASE("the grace factor makes the ram no harder than the click") {
    // 클릭으로 맞힐 수 있는 놈은 몸으로도 맞힐 수 있어야 한다. graceScale < 1 이면
    // 아이는 '분명히 닿았는데 안 됐다'를 겪는다.
    REQUIRE(kC.graceScale >= 1.f);
}

TEST_CASE("there is no angle gate and no dwell timer in this header") {
    // 구조로 보장한다: EvaluateRam은 '지금 이 순간'만 받는다. 시간 인자도,
    // 각도 인자도 없으므로 그런 조건을 넣는 코드를 쓰는 것 자체가 불가능하다.
    std::vector<RamTarget> targets{Fish(400.f, {0.f, 0.f})};
    Rammer me;
    me.depth = 220.f;
    me.maxSpeed = 90.f;
    me.nose = {0.f, 0.f};
    // 옆에서 들이받는다(진행 방향이 상대의 장축과 수직). 그래도 잡힌다.
    me.velocity = {0.f, kDashSpeed};
    const RamResult r = EvaluateRam(kCam, me, targets.data(), targets.size(), kC);
    REQUIRE(r.outcome == RamOutcome::Catch);
}
