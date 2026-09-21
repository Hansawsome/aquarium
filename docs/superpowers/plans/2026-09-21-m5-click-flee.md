# M5 — 클릭 도망·회복·애니메이션 구현 계획 (F-09 ~ F-13)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 클릭 한 번에 **가장 앞의 물고기 한 마리**가 클릭 지점에서 멀어지며 가속해 0.8초 도망하고, 1.2초에 걸쳐 원래 제어 상태로 돌아오게 한다. 빈 바다 클릭은 아무 물고기에도 영향을 주지 않는다. 규칙 90 → **103**, Automation 47 → **60**.

**Architecture:** 도망 로직은 이미 있는 `rules/include/aquarium/Flee.h`를 **재사용**한다. 엔진에 상태 기계를 다시 쓰지 않는다. 규칙 계층에 새로 넣는 것은 정확히 세 가지뿐이다 — 퇴화 입력에서의 **비영 방향 보장**, **속도 배율**(F-10의 "가속"), **해석적 적중 판정**(`PickFrontmostHit`). Unreal 쪽은 역투영 1회로 만든 월드 광선을 넘기고 결과를 소비할 뿐이다. 라인 트레이스는 쓰지 않는다(이유는 설계 문서). 판정은 **클립**이 1차, 스틸은 보조.

**Tech Stack:** C++17 규칙 계층 + Catch2, Unreal 5.8.2 (C++20 모듈 + 에디터 Python), ffmpeg.

**설계 문서:** [`docs/superpowers/specs/2026-09-21-m5-click-flee-design.md`](../specs/2026-09-21-m5-click-flee-design.md)

**공통 명령**:
- 규칙 테스트: `cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j && ctest --test-dir build`
- UE 빌드(**반드시 두 번**): `"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex`
- Automation: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "Test Completed|Tests Failed"`

**모든 태스크에 걸리는 규약 여덟 가지 (전부 이 프로젝트가 실제로 대가를 치른 것):**

1. **이 ctest는 `100% tests passed out of N` 형식을 쓴다.** `"0 tests failed out of"`를 grep하면 **아무것도 검증하지 않은 채 통과한다.** 기대 문자열은 언제나 `100% tests passed` + 개수로 쓴다.
2. **규칙 테스트 mtime 함정** — 같은 초 안에 다시 빌드하면 `make`가 재컴파일을 건너뛰고 ctest가 **낡은 결과**를 보고한다. RED를 확인할 때는 편집 후 `touch`로 mtime을 올리거나 빌드 사이에 1초를 둔다(각 RED 스텝에 명령으로 박아 두었다).
3. **UE 모듈은 자동화 전에 두 번 빌드한다.** UBT가 `UnrealEditor.modules`를 한 빌드 늦게 쓴다.
4. **에디터 Python은 `-FullStdOutLogOutput` 없이 돌리면 아무것도 출력하지 않는다** — `REEF_OK`도, `AssertionError`도. 그런데 레벨은 덮어쓴다. **종료 코드는 무관한 `GameFeatureData` 오류 때문에 항상 1이다. 판정은 `*_OK` grep으로만 한다.** (M5는 레벨을 바꾸지 않으므로 Python은 돌리지 않지만, 돌리게 되면 이 규약이 적용된다.)
5. **머티리얼은 조용히 실패하고 회색 기본 머티리얼로 떨어지면서 에디터 뷰포트에서는 멀쩡해 보인다.** 캡처 실행 로그에서 `Failed to compile Material` 0건을 반드시 grep한다. **`-nullrhi`가 아닌 실제 게임 실행 로그여야 한다.**
6. **테스트는 아무것도 검사하지 않으면서 통과할 수 있다.** `PropMaterialsCompile`은 Success를 내면서 `checked 0 of 13`을 찍었고, `SchoolMatesPullTogether`는 무리를 완전히 끈 채로 통과했으며, `UpVectorStaysUpright`는 구조상 빨간불이 될 수 없다. **이 계획의 모든 신규 테스트는 빨간불을 먼저 확인한다. 계획이 제안한 변이로 빨간불이 켜지지 않으면 그것은 통과의 증거가 아니다 — 빨간불이 켜지는 변이를 찾거나, 찾지 못했다는 사실을 후속 항목에 적는다.**
7. **같은 규칙을 두 군데 두면 검증이 결함을 영원히 못 잡는다.** 물고기 적중 반경은 **액터 바운드에서 파생**하고, 테스트 기대값은 **규칙 계층 함수·파라미터에서 파생**한다. 반경도 지속시간도 리터럴로 베끼지 않는다.
8. **`-AquariumAutoInput` / `-AquariumAutoClick` 파서는 알아볼 수 없는 토큰에 경고만 찍고 조용히 무시한다.** M4c에서 계획서가 지어낸 토큰이 전부 무효라 "입력이 하나도 없는 그럴듯한 클립"을 만들 뻔했다. **캡처 하네스는 경고 0건과 `armed N`을 단언한다.**

---

### Task 0: 기준 확인과 보관

**Files:** 없음 (읽기 전용)

- [ ] **Step 1: 브랜치와 작업 트리 확인**

```bash
cd /Users/hans/dev/aquarium
git status --short && git rev-parse --abbrev-ref HEAD && git rev-parse --short HEAD
```
기대: 첫 줄 출력 없음, `feat/m5-click-flee`, `f8ec153`.

- [ ] **Step 2: 현재 규칙 90개 통과 확인**

```bash
cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 90`. (`0 tests failed out of`를 찾지 말 것 — 규약 1.)

- [ ] **Step 3: 도망이 엔진에 정말 없는지 직접 확인**

```bash
cd /Users/hans/dev/aquarium && grep -rn "Flee\|flee\|LeftMouseButton" unreal/Aquarium/Source/ | wc -l
```
기대: `0`. 0이 아니면 이 계획의 전제가 틀린 것이므로 진행하지 말고 보고한다.

- [ ] **Step 4: 기준 산출물 보관**

```bash
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m5_before
mkdir -p "$B"
cd /Users/hans/dev/aquarium
cp docs/reviews/2026-09-21-m4c-scene.png "$B/before-scene.png"
cp docs/reviews/2026-09-21-m4c-reef.mp4 "$B/before-reef.mp4"
cp docs/reviews/2026-09-21-m4c-perf.md "$B/before-perf.md"
ls -l "$B"
```

- [ ] **Step 5: 개선 전 클립을 직접 본다** — `$B/before-reef.mp4`. 확인할 것: **지금은 클릭해도 아무 일이 없다**는 것과, 배경 물고기의 순항 속도가 눈에 어느 정도로 보이는지. 뒤에서 "도망이 놀란 것처럼 보인다"를 주장할 때의 기준이다.

---

### Task 1: 규칙 계층 — 비영 방향 보장과 속도 배율 (`Flee.h`)

**Files:** Modify `rules/include/aquarium/Flee.h`, Modify `tests/test_flee.cpp`

F-10의 "가속"과, 세 폴백이 전부 0일 때 **놀란 물고기가 멈춰 버리는** 결함을 없앤다.

- [ ] **Step 1 (RED): 테스트를 먼저 추가한다**

`tests/test_flee.cpp` **맨 끝에** 아래를 덧붙인다(기존 6개 테스트는 건드리지 않는다).

```cpp
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
```

```bash
cd /Users/hans/dev/aquarium && touch tests/test_flee.cpp && cmake --build build -j 2>&1 | tail -5
```
기대: **컴파일 실패** — `no member named 'SpeedScale'`, `no member named 'fleeSpeedScale'`. 이것이 RED다.

- [ ] **Step 2 (GREEN): `Flee.h`를 고친다**

`rules/include/aquarium/Flee.h`의 `FleeParams`와 `ComputeFleeDirection`을 아래로 바꾸고, `FleeStateMachine`에 `SpeedScale`을 더한다.

```cpp
struct FleeParams {
    float fleeDuration = 0.8f;
    float recoverDuration = 1.2f;
    // F-10 says the fish rotates AND accelerates. Direction alone reads as a calm course change
    // at a background fish's 40 cm/s, not as being startled. accel is deliberately NOT scaled:
    // raising acceleration too would spike the velocity on the first frame, which is exactly the
    // "sliding" F-13 forbids.
    float fleeSpeedScale = 2.2f;
};

// Last-resort direction when every fallback is degenerate. Any fixed unit vector will do; what
// matters is that this function never returns {0,0}, because the engine reads a zero desired
// direction as "no input" and decelerates -- a fish that stops when startled.
inline Vec2 DefaultFleeDirection() { return {1.f, 0.f}; }

// Direction away from touch; falls back to current heading, then to fallbackDir, then to a fixed
// unit vector. Guaranteed to be a unit vector.
inline Vec2 ComputeFleeDirection(Vec2 touch, Vec2 fishPos, Vec2 velocity, Vec2 fallbackDir) {
    const Vec2 away = (fishPos - touch).Normalized();
    if (away.Length() > 0.f) return away;
    const Vec2 heading = velocity.Normalized();
    if (heading.Length() > 0.f) return heading;
    const Vec2 fb = fallbackDir.Normalized();
    if (fb.Length() > 0.f) return fb;
    return DefaultFleeDirection();
}
```

`FleeStateMachine`의 `public:` 구역에 추가한다.

```cpp
    // Multiplier applied to the fish's max speed (F-10). Flat through the flee, then linear back
    // to 1 across recovery: dropping to 1 in one step at the end of the flee reads as a brake.
    float SpeedScale(const FleeParams& p) const {
        if (state_ == BehaviorState::Fleeing) return p.fleeSpeedScale;
        if (state_ == BehaviorState::Recovering && recoverDuration_ > 0.f) {
            const float t = timer_ / recoverDuration_;       // 1 at the start, 0 at the end
            const float clamped = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
            return 1.f + (p.fleeSpeedScale - 1.f) * clamped;
        }
        return 1.f;
    }
```

```bash
cd /Users/hans/dev/aquarium && touch tests/test_flee.cpp && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 95`.

- [ ] **Step 3: 물리는지 확인한다 (변이 검사)** — `DefaultFleeDirection()`의 반환을 잠시 `{0.f, 0.f}`로 바꾸고 재빌드·재실행한다.

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/return {1.f, 0.f}; }/return {0.f, 0.f}; }/' rules/include/aquarium/Flee.h && touch tests/test_flee.cpp && cmake --build build -j >/dev/null 2>&1 && ctest --test-dir build 2>&1 | tail -3
```
기대: **실패 1건** (`flee direction is never a zero vector`). 실패하지 않으면 테스트가 아무것도 검사하지 않는 것이므로 멈추고 원인을 찾는다. 확인 후 되돌린다.

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/return {0.f, 0.f}; }/return {1.f, 0.f}; }/' rules/include/aquarium/Flee.h && touch tests/test_flee.cpp && cmake --build build -j >/dev/null 2>&1 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 95`.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules/include/aquarium/Flee.h tests/test_flee.cpp && git commit -m "$(cat <<'EOF'
feat(rules): 도망 방향 비영 보장과 속도 배율 추가 (F-10)

세 폴백이 모두 퇴화했을 때 0 벡터를 돌려주면 엔진이 감속으로 읽어
놀란 물고기가 멈춘다. 최종 폴백을 두고 단위 벡터를 보장한다.
F-10의 "가속"을 위해 도망 중 2.2배, 회복 중 선형 복귀하는 속도 배율을
더했다. 규칙 90 -> 95.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 2: 규칙 계층 — 해석적 적중 판정 (`PickFrontmostHit`)

**Files:** Modify `rules/include/aquarium/Flee.h`, Modify `tests/test_flee.cpp`

- [ ] **Step 1 (RED): 테스트를 먼저 추가한다**

`tests/test_flee.cpp` 맨 위의 include에 한 줄을 더한다.

```cpp
#include "aquarium/SwimPlane.h"   // Vec3
```

그리고 파일 끝에 덧붙인다.

```cpp
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
```

```bash
cd /Users/hans/dev/aquarium && touch tests/test_flee.cpp && cmake --build build -j 2>&1 | tail -5
```
기대: **컴파일 실패** — `no type named 'ClickTarget'`, `no member named 'PickFrontmostHit'`.

- [ ] **Step 2 (GREEN): `Flee.h`에 구현을 더한다**

`rules/include/aquarium/Flee.h`의 include를 바꾸고(`Vec3`가 필요하다), 파일의 `} // namespace aquarium` **앞에** 아래를 넣는다.

```cpp
#include "aquarium/SwimPlane.h"   // Vec2 and Vec3
```

```cpp
// One fish as a click can see it (F-09). `depth` is the fish's plane X, which is constant for the
// fish's whole life; `center` is in the SHARED swim frame (worldY, worldZ) -- the same frame
// BoidNeighbor::position uses, so no new coordinate system is introduced. halfWidth/halfHeight
// come from the actor's real rendered bounds on the engine side; nothing here knows what a
// species is or how big one should be.
struct ClickTarget {
    float depth = 0.f;
    Vec2 center;
    float halfWidth = 0.f;
    float halfHeight = 0.f;
};

// Index of the frontmost target the ray passes through, or -1 for empty water (F-09).
//
// Every fish lives on a plane of constant world X and the camera looks along +X, so plane X is
// exactly screen depth and each plane costs one divide to intersect exactly. This is the same
// trick Obstacles.h uses to precompute per-plane obstacle lists, and it is why the click test is
// a handful of flops per fish rather than a physics query. A line trace is NOT used on purpose:
// the auto-generated physics assets are far fatter than the meshes (43.9 x 26.0 cm of capsule
// around a 25.0 x 3.6 cm blue tang), so a trace would hit empty water and, worse, would get the
// FRONTMOST answer wrong.
inline int PickFrontmostHit(Vec3 rayOrigin, Vec3 rayDir, const ClickTarget* targets,
                            std::size_t count) {
    if (targets == nullptr || count == 0) return -1;
    if (rayDir.x <= 1e-4f) return -1;          // parallel to the planes, or pointing backwards
    int best = -1;
    float bestDepth = 0.f;
    for (std::size_t i = 0; i < count; ++i) {
        const ClickTarget& t = targets[i];
        if (t.halfWidth <= 0.f || t.halfHeight <= 0.f) continue;
        const float s = (t.depth - rayOrigin.x) / rayDir.x;
        if (s <= 0.f) continue;                // the plane is behind the click origin
        const float hy = rayOrigin.y + rayDir.y * s;
        const float hz = rayOrigin.z + rayDir.z * s;
        const float u = (hy - t.center.x) / t.halfWidth;
        const float v = (hz - t.center.y) / t.halfHeight;
        if (u * u + v * v > 1.f) continue;     // ellipse, not a circle: a fish is long and thin
        if (best < 0 || t.depth < bestDepth) { // frontmost = smallest plane X
            best = static_cast<int>(i);
            bestDepth = t.depth;
        }
    }
    return best;
}
```

`<cstddef>`가 필요하므로 파일 맨 위 include 목록에 `#include <cstddef>`도 더한다.

```bash
cd /Users/hans/dev/aquarium && touch tests/test_flee.cpp && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 103`.

- [ ] **Step 3: 물리는지 확인한다 (변이 검사 2회)**

변이 A — "가장 앞" 대신 "처음 찾은 것"을 쓰게 만든다.

```bash
cd /Users/hans/dev/aquarium
sed -i '' 's/if (best < 0 || t.depth < bestDepth)/if (best < 0)/' rules/include/aquarium/Flee.h
touch tests/test_flee.cpp && cmake --build build -j >/dev/null 2>&1 && ctest --test-dir build 2>&1 | tail -3
```
기대: **실패 1건** (`three overlapping fish`).

변이 B — 타원을 원으로 바꾼다.

```bash
cd /Users/hans/dev/aquarium
sed -i '' 's/if (best < 0)/if (best < 0 || t.depth < bestDepth)/' rules/include/aquarium/Flee.h
sed -i '' 's|const float v = (hz - t.center.y) / t.halfHeight;|const float v = (hz - t.center.y) / t.halfWidth;|' rules/include/aquarium/Flee.h
touch tests/test_flee.cpp && cmake --build build -j >/dev/null 2>&1 && ctest --test-dir build 2>&1 | tail -3
```
기대: **실패 1건** (`the hit shape is an ellipse, not a circle`). 되돌린다.

```bash
cd /Users/hans/dev/aquarium
sed -i '' 's|const float v = (hz - t.center.y) / t.halfWidth;|const float v = (hz - t.center.y) / t.halfHeight;|' rules/include/aquarium/Flee.h
touch tests/test_flee.cpp && cmake --build build -j >/dev/null 2>&1 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed out of 103`. 두 변이 중 하나라도 빨간불이 되지 않으면 **멈추고** 그 사실을 후속 항목에 적는다.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules/include/aquarium/Flee.h tests/test_flee.cpp && git commit -m "$(cat <<'EOF'
feat(rules): 광선-평면 해석적 적중 판정 추가 (F-09)

물고기는 전부 X가 고정된 YZ 평면 위에 있고 카메라는 +X를 본다. 평면
하나당 나눗셈 한 번으로 정확한 교차점이 나오고, 평면 안에서 타원으로
판정한 뒤 X가 가장 작은 것 하나만 돌려준다. 빈 바다는 -1.
라인 트레이스를 쓰지 않는 이유는 설계 문서에 적었다. 규칙 95 -> 103.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

> **개수 확인**: Task 1에서 5개, Task 2에서 8개를 더해 규칙 테스트는 90 → **103개**다. 뒤의 모든 기대 문자열과 문서 갱신은 103을 쓴다.

---

### Task 3: `AFishActor` — 도망 층과 속도 배율 (F-10 · F-11 · F-07)

**Files:** Modify `unreal/Aquarium/Source/Aquarium/FishActor.h`, Modify `unreal/Aquarium/Source/Aquarium/FishActor.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 1 (RED): Automation 테스트 3개를 먼저 쓴다**

`Tests/FishActorTests.cpp` 끝에 덧붙인다. 파일 상단 include에 `#include "aquarium/Flee.h"`를 더한다(없으면).

```cpp
// F-10/F-11: a clicked fish turns away from the touch point, speeds up, and comes back.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFleesFromTouch, "Aquarium.Fish.FleesFromTouchPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFleesFromTouch::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	Fish->InitializeSwim();
	// Let it get moving first, so the flee has to overcome a real velocity.
	for (int32 i = 0; i < 60; ++i) { Fish->StepSwim(1.f / 60.f); }

	// Touch 20 cm to the fish's screen-left: it must end up moving screen-right.
	const FVector Touch = Fish->GetActorLocation() - FVector(0.f, 20.f, 0.f);
	const float CruiseSpeed = Fish->CurrentSpeed();
	Fish->ApplyFleeFrom(Touch);
	TestTrue(TEXT("state is Fleeing right after the touch"),
		Fish->FleeState() == aquarium::BehaviorState::Fleeing);

	for (int32 i = 0; i < 30; ++i) { Fish->StepSwim(1.f / 60.f); }   // 0.5 s into the flee
	TestTrue(TEXT("moving away from the touch (screen right = +Y)"),
		Fish->GetActorLocation().Y > Touch.Y);
	TestTrue(TEXT("flee is faster than cruising"), Fish->CurrentSpeed() > CruiseSpeed * 1.2f);

	for (int32 i = 0; i < 20; ++i) { Fish->StepSwim(1.f / 60.f); }   // total 0.833 s -> Recovering
	TestTrue(TEXT("recovering after 0.8 s"),
		Fish->FleeState() == aquarium::BehaviorState::Recovering);
	for (int32 i = 0; i < 80; ++i) { Fish->StepSwim(1.f / 60.f); }   // total 2.16 s -> Normal
	TestTrue(TEXT("normal after 2.0 s"),
		Fish->FleeState() == aquarium::BehaviorState::Normal);
	return true;
}

// F-07: the boundary rule still gets the last word while a fish is fleeing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFleeStaysInsideArea, "Aquarium.Fish.FleeStaysInsideArea",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFleeStaysInsideArea::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	Fish->PlaneHalfWidth = 60.f;
	Fish->PlaneHalfHeight = 40.f;
	Fish->InitializeSwim();
	// Click repeatedly from the opposite side so the flee direction always points at a wall.
	float WorstY = 0.f, WorstZ = 0.f;
	for (int32 i = 0; i < 600; ++i)
	{
		if (i % 120 == 0)
		{
			Fish->ApplyFleeFrom(Fish->GetActorLocation() - FVector(0.f, 30.f, 20.f));
		}
		Fish->StepSwim(1.f / 60.f);
		WorstY = FMath::Max(WorstY, FMath::Abs(Fish->GetActorLocation().Y - Fish->PlaneOrigin.Y));
		WorstZ = FMath::Max(WorstZ, FMath::Abs(Fish->GetActorLocation().Z - Fish->PlaneOrigin.Z));
	}
	// Derived from the actor's own half extents, never from a literal.
	TestTrue(FString::Printf(TEXT("stays inside half width (%.2f <= %.2f)"), WorstY, Fish->PlaneHalfWidth),
		WorstY <= Fish->PlaneHalfWidth + KINDA_SMALL_NUMBER);
	TestTrue(FString::Printf(TEXT("stays inside half height (%.2f <= %.2f)"), WorstZ, Fish->PlaneHalfHeight),
		WorstZ <= Fish->PlaneHalfHeight + KINDA_SMALL_NUMBER);
	return true;
}

// F-12: a background fish goes back to wandering, not to a frozen heading.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorBackgroundResumesWander, "Aquarium.Fish.BackgroundResumesWanderAfterFlee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorBackgroundResumesWander::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Ref = World->SpawnActor<AFishActor>();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	for (AFishActor* F : {Ref, Fish})
	{
		F->PlaneOrigin = FVector(400.f, 0.f, 100.f);
		F->Seed = 7;
		F->InitializeSwim();
	}
	Fish->ApplyFleeFrom(Fish->GetActorLocation() - FVector(0.f, 25.f, 0.f));
	for (int32 i = 0; i < 600; ++i) { Ref->StepSwim(1.f / 60.f); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("back to Normal"), Fish->FleeState() == aquarium::BehaviorState::Normal);
	// Both fish have the same seed and the same wander target sequence, so once the flee is over
	// the disturbed fish must be steering toward a live target again: its speed must be back to
	// the ordinary cruising band rather than stuck at the flee burst.
	TestTrue(FString::Printf(TEXT("speed back in the cruise band (%.2f vs %.2f)"),
		Fish->CurrentSpeed(), Ref->CurrentSpeed()),
		Fish->CurrentSpeed() <= Ref->CurrentSpeed() + 1.f);
	TestTrue(TEXT("still swimming, not stalled"), Fish->CurrentSpeed() > 1.f);
	return true;
}
```

```bash
cd /Users/hans/dev/aquarium && "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -5
```
기대: **컴파일 실패** — `no member named 'ApplyFleeFrom'` / `'FleeState'`.

- [ ] **Step 2 (GREEN): `FishActor.h`**

include 목록에 더한다.

```cpp
#include "aquarium/Flee.h"
```

`public:` 구역에 더한다(`SetInputDirection` 바로 위).

```cpp
	// F-09..F-12: startles this fish away from a world-space touch point. The point is expected to
	// be on (or near) this fish's swim plane; only its Y and Z are used, because the plane's X is
	// what made the click hit this fish in the first place. Re-entrant by design: the rules layer
	// decides what a second touch means (ignored mid-flee, restarts during recovery).
	void ApplyFleeFrom(const FVector& WorldTouch);
	aquarium::BehaviorState FleeState() const { return Flee.State(); }
	// This fish as a click can see it, in the shared swim frame. Half extents are DERIVED from the
	// rendered bounds (so the player fish's 34 cm normalization scale is included automatically);
	// no per-species radius table is copied into C++.
	aquarium::ClickTarget AsClickTarget() const;
```

`private:` 구역에 더한다(`BoidsParamsValue` 옆).

```cpp
	aquarium::FleeStateMachine Flee;
	aquarium::FleeParams FleeParamsValue;
```

- [ ] **Step 3 (GREEN): `FishActor.cpp`**

`InitializeSwim()`의 `Wander.Emplace(...)` 바로 앞에 한 줄을 넣는다(재입장 시 도망 상태가 이월되지 않게).

```cpp
	Flee = aquarium::FleeStateMachine();
```

`StepSwim`의 `Wander->Update(...)` / `Desired` 계산 **직후**, 무리 블록 **앞**에 도망 층을 끼운다.

```cpp
	// FLEE LAYER (F-10/F-11/F-12). Placed here because fleeing REPLACES the answer to "where do I
	// want to go", exactly like arrow-key input and wander do -- it is not a correction applied to
	// that answer. Everything after this point (obstacles, boundary, StepMotion, Clamp) still runs,
	// so a startled fish can neither swim through a rock (M4c) nor leave the visible area (F-07,
	// and the M3 guarantee that the wall always gets the last word).
	Flee.Step(DeltaSeconds);
	const bool bFleeing = Flee.State() == aquarium::BehaviorState::Fleeing;
	if (bFleeing)
	{
		Desired = Flee.FleeDirection();
	}
	// The speed burst is what makes a course change read as being startled (F-10). Only maxSpeed
	// is scaled; scaling accel as well would spike the velocity on the first frame, which is the
	// "sliding" F-13 forbids.
	MotionParamsValue.maxSpeed = (bPlayerControlled ? MaxSpeed : MaxSpeed) * Flee.SpeedScale(FleeParamsValue);
```

무리 블록의 조건에 도망 제외를 더한다.

```cpp
	if (!bPlayerControlled && !bIsPlayerFish && !bFleeing)
```

> 이 한 줄이 설계 결정 "도망 중에는 무리를 건너뛴다"다. 응집이 도망을 자기 무리 쪽으로 도로 끌어당기면 아이가 클릭의 결과를 읽을 수 없다.

`MaxSpeed` 대입이 `InitializeSwim`의 값을 덮어쓰므로, `InitializeSwim`의 `MotionParamsValue.maxSpeed = MaxSpeed;`는 그대로 두고 위 줄이 매 틱 다시 계산한다. 파일 끝(`ComputeSpeciesKey` 옆)에 두 함수를 더한다.

```cpp
void AFishActor::ApplyFleeFrom(const FVector& WorldTouch)
{
	// The rules layer works in plane-local 2D; the touch arrives in world space.
	const aquarium::Vec2 TouchLocal{
		static_cast<float>(WorldTouch.Y) - Plane.origin.y,
		static_cast<float>(WorldTouch.Z) - Plane.origin.z};
	// F-10's "otherwise a valid direction toward the screen centre". The swim area is centred on
	// the plane origin and that origin sits on the camera axis, so the direction toward the plane
	// centre IS the direction toward the screen centre. When the fish is exactly at the centre
	// this is zero too, and the rules layer's last-resort fallback takes over.
	const aquarium::Vec2 TowardCentre = (aquarium::Vec2{0.f, 0.f} - Motion.position).Normalized();
	Flee.Touch(TouchLocal, Motion.position, Motion.velocity, TowardCentre, FleeParamsValue);
}

aquarium::ClickTarget AFishActor::AsClickTarget() const
{
	aquarium::ClickTarget T;
	T.depth = Plane.origin.x;
	// Same shared frame as AsNeighbor(): every plane uses right = +Y and up = +Z.
	T.center = {Plane.origin.y + Motion.position.x, Plane.origin.z + Motion.position.y};
	// Derived from what is actually drawn, including the player fish's normalization scale.
	const FBoxSphereBounds B = Body->CalcBounds(Body->GetComponentTransform());
	T.halfWidth = static_cast<float>(B.BoxExtent.Y);
	T.halfHeight = static_cast<float>(B.BoxExtent.Z);
	return T;
}
```

- [ ] **Step 4 (GREEN): 빌드 두 번 + Automation**

```bash
cd /Users/hans/dev/aquarium
for i in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -2; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: `50`.

- [ ] **Step 5: 물리는지 확인한다 (변이 검사)** — 도망 층의 `Desired = Flee.FleeDirection();`을 잠시 주석 처리하고 위 Automation을 다시 돌린다. 기대: `Aquarium.Fish.FleesFromTouchPoint` **실패**. 되돌린다. 빨간불이 안 나오면 멈추고 원인을 찾는다.

- [ ] **Step 6: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/FishActor.h unreal/Aquarium/Source/Aquarium/FishActor.cpp unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp && git commit -m "$(cat <<'EOF'
feat(unreal): 물고기에 도망 층 연결 (F-10, F-11, F-07)

규칙 순서를 입력/Wander -> 도망 -> 무리 -> 장애물 -> 경계로 고정했다.
도망은 원하는 방향을 교체하므로 입력과 같은 층이고, 뒤의 장애물·경계는
그대로 남아 벽이 마지막에 이긴다. 도망 중에는 무리를 건너뛴다.
Automation 47 -> 50.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 4: 내 물고기 — 방향키와의 우선순위 (F-12)

**Files:** Modify `unreal/Aquarium/Source/Aquarium/FishActor.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 1 (RED): 테스트 2개를 먼저 쓴다**

`Tests/FishActorTests.cpp` 끝에 덧붙인다.

```cpp
// F-12: while fleeing, the flee beats the arrow keys; from recovery the CURRENT key applies.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFleeBeatsArrowKeys, "Aquarium.Fish.FleeBeatsArrowKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFleeBeatsArrowKeys::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(220.f, 0.f, 105.f);
	Fish->bIsPlayerFish = true;
	Fish->bPlayerControlled = true;
	Fish->InitializeSwim();
	// The child is holding LEFT (screen -Y) the whole time.
	Fish->SetInputDirection(FVector2D(-1.f, 0.f));
	for (int32 i = 0; i < 30; ++i) { Fish->StepSwim(1.f / 60.f); }
	const double YBeforeClick = Fish->GetActorLocation().Y;

	// The child clicks their OWN fish, on its left side, so the flee wants to go RIGHT.
	Fish->ApplyFleeFrom(Fish->GetActorLocation() - FVector(0.f, 15.f, 0.f));
	for (int32 i = 0; i < 40; ++i)
	{
		Fish->SetInputDirection(FVector2D(-1.f, 0.f));   // still held, as the controller would
		Fish->StepSwim(1.f / 60.f);
	}
	TestTrue(TEXT("flee wins over the held key"), Fish->GetActorLocation().Y > YBeforeClick);

	// Run to the end of the flee, then keep holding LEFT through recovery.
	for (int32 i = 0; i < 20; ++i) { Fish->SetInputDirection(FVector2D(-1.f, 0.f)); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("recovering"), Fish->FleeState() == aquarium::BehaviorState::Recovering);
	const double YAtRecovery = Fish->GetActorLocation().Y;
	for (int32 i = 0; i < 60; ++i) { Fish->SetInputDirection(FVector2D(-1.f, 0.f)); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("the held key steers again from recovery"), Fish->GetActorLocation().Y < YAtRecovery);
	return true;
}

// F-12: a key RELEASED during the flee must not come back to life at recovery.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorRecoveryUsesCurrentInput, "Aquarium.Fish.RecoveryUsesCurrentInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorRecoveryUsesCurrentInput::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(220.f, 0.f, 105.f);
	Fish->bIsPlayerFish = true;
	Fish->bPlayerControlled = true;
	Fish->InitializeSwim();
	Fish->SetInputDirection(FVector2D(0.f, 1.f));                    // holding UP
	for (int32 i = 0; i < 30; ++i) { Fish->StepSwim(1.f / 60.f); }
	Fish->ApplyFleeFrom(Fish->GetActorLocation() + FVector(0.f, 0.f, 15.f));  // flee downward
	// The child lets go during the flee.
	for (int32 i = 0; i < 60; ++i) { Fish->SetInputDirection(FVector2D::ZeroVector); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("recovering"), Fish->FleeState() == aquarium::BehaviorState::Recovering);
	const float SpeedAtRecovery = Fish->CurrentSpeed();
	for (int32 i = 0; i < 72; ++i) { Fish->SetInputDirection(FVector2D::ZeroVector); Fish->StepSwim(1.f / 60.f); }
	TestTrue(FString::Printf(TEXT("coasts to a stop, no resurrected key (%.2f < %.2f)"),
		Fish->CurrentSpeed(), SpeedAtRecovery),
		Fish->CurrentSpeed() < SpeedAtRecovery * 0.5f);
	return true;
}
```

```bash
cd /Users/hans/dev/aquarium
for i in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -2; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "FleeBeatsArrowKeys|RecoveryUsesCurrentInput"
```
기대: `Aquarium.Fish.FleeBeatsArrowKeys` **실패**. Task 3의 도망 층은 `bPlayerControlled`일 때 `Desired`가 `InputDirection`으로 시작하지만 그 뒤 `Desired = Flee.FleeDirection()`으로 덮이므로 **첫 테스트는 이미 통과할 수 있다.** 통과한다면 그 사실을 기록하고, 이 태스크는 통과를 **확인**하는 태스크가 된다 — 단, 두 번째 테스트는 반드시 돌려서 통과를 확인한다.

- [ ] **Step 2 (GREEN): 필요한 경우에만 고친다**

두 테스트가 모두 통과하면 코드 변경 없이 다음 스텝으로 간다. 실패하면 `StepSwim`의 도망 층이 **플레이어 물고기에도 적용되는지** 확인한다(Task 3의 코드는 `bPlayerControlled` 분기 밖에 있으므로 적용되어야 한다).

- [ ] **Step 3: 물리는지 확인한다 (변이 검사)** — 도망 층의 조건을 잠시 `if (bFleeing && !bPlayerControlled)`로 바꾸고 Automation을 돌린다. 기대: `Aquarium.Fish.FleeBeatsArrowKeys` **실패**(내 물고기가 도망하지 않는다). 되돌린다.

- [ ] **Step 4: 전체 Automation 확인과 커밋**

```bash
cd /Users/hans/dev/aquarium && "/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: `52`.

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp unreal/Aquarium/Source/Aquarium/FishActor.cpp && git commit -m "$(cat <<'EOF'
test(unreal): 내 물고기의 도망/방향키 우선순위 고정 (F-12)

도망 0.8초 동안은 눌린 방향키를 무시하고, 회복 시작 시점부터 그 순간
실제로 눌려 있는 키가 다시 적용된다. 도망 중에 키를 놓았으면 되살아나지
않고 감속한다. Automation 50 -> 52.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 5: `UFishSchoolSubsystem` — 클릭 대상 선택 (F-09)

**Files:** Modify `unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.h`, Modify `unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 1 (RED): 테스트 2개를 먼저 쓴다**

`Tests/FishActorTests.cpp` 끝에 덧붙인다(파일 상단 include에 `#include "FishSchoolSubsystem.h"`가 이미 있다).

```cpp
// F-09: exactly one fish per click, and it is the frontmost one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishClickPicksFrontmost, "Aquarium.Fish.ClickPicksFrontmostFish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishClickPicksFrontmost::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	TestNotNull(TEXT("subsystem"), School);

	// Three fish stacked on the same screen point at different plane depths.
	TArray<AFishActor*> Fish;
	for (float Depth : {700.f, 330.f, 500.f})
	{
		AFishActor* F = World->SpawnActor<AFishActor>();
		F->PlaneOrigin = FVector(Depth, 0.f, 100.f);
		F->InitializeSwim();
		School->Register(F);
		Fish.Add(F);
	}
	// A ray from the camera position straight at that point.
	FVector Hit = FVector::ZeroVector;
	AFishActor* Picked = School->PickFrontmostHit(FVector(0.f, 0.f, 100.f), FVector(1.f, 0.f, 0.f), Hit);
	TestTrue(TEXT("something was hit"), Picked != nullptr);
	TestEqual(TEXT("the frontmost plane wins"), Picked, Fish[1]);   // depth 330
	TestEqual(TEXT("hit point is on that plane"), Hit.X, 330.f, 0.1f);
	return true;
}

// F-09: empty water affects nothing at all.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishClickOnEmptyWater, "Aquarium.Fish.ClickOnEmptyWaterHitsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishClickOnEmptyWater::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	AFishActor* F = World->SpawnActor<AFishActor>();
	F->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	F->InitializeSwim();
	School->Register(F);

	FVector Hit = FVector::ZeroVector;
	// 3 m above every fish: nothing to hit.
	AFishActor* Picked = School->PickFrontmostHit(FVector(0.f, 0.f, 400.f), FVector(1.f, 0.f, 0.f), Hit);
	TestNull(TEXT("empty water hits nothing"), Picked);
	TestTrue(TEXT("no fish was disturbed"), F->FleeState() == aquarium::BehaviorState::Normal);
	return true;
}
```

```bash
cd /Users/hans/dev/aquarium && "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -5
```
기대: **컴파일 실패** — `no member named 'PickFrontmostHit'`.

- [ ] **Step 2 (GREEN): `FishSchoolSubsystem.h`**

`Neighbors()` 선언 아래에 더한다.

```cpp
	// F-09: the frontmost registered fish the ray passes through, or nullptr for empty water.
	// The picking rule itself lives in aquarium::PickFrontmostHit; this only supplies the list,
	// exactly as Neighbors() supplies the boid list. OutHit is the world-space point on that
	// fish's plane, which the caller hands straight to AFishActor::ApplyFleeFrom.
	AFishActor* PickFrontmostHit(const FVector& RayOrigin, const FVector& RayDir, FVector& OutHit);

	// Dev-only toggle: skips click handling and the flee layer entirely (performance attribution).
	bool bFleeEnabled = true;
```

`#include "aquarium/Flee.h"` 를 include 목록에 더한다.

- [ ] **Step 3 (GREEN): `FishSchoolSubsystem.cpp`**

`Initialize`의 토글 블록에 한 줄을 더한다.

```cpp
	bFleeEnabled = !ParseDisableFlag(FCommandLine::Get(), TEXT("AquariumNoFlee"));
```

`Neighbors()` 바로 아래에 더한다.

```cpp
AFishActor* UFishSchoolSubsystem::PickFrontmostHit(const FVector& RayOrigin, const FVector& RayDir,
                                                   FVector& OutHit)
{
	OutHit = FVector::ZeroVector;
	if (!bFleeEnabled)
	{
		return nullptr;
	}
	Fishes.RemoveAll([](const TWeakObjectPtr<AFishActor>& P) { return !P.IsValid(); });
	// Built fresh per click rather than cached: a click happens a few times a second at most, and
	// a cached list would have to be invalidated on every move. This is the whole per-click cost.
	TArray<AFishActor*> Actors;
	std::vector<aquarium::ClickTarget> Targets;
	Actors.Reserve(Fishes.Num());
	Targets.reserve(static_cast<size_t>(Fishes.Num()));
	for (const TWeakObjectPtr<AFishActor>& P : Fishes)
	{
		Actors.Add(P.Get());
		Targets.push_back(P->AsClickTarget());
	}
	const FVector Dir = RayDir.GetSafeNormal();
	const int Index = aquarium::PickFrontmostHit(
		{static_cast<float>(RayOrigin.X), static_cast<float>(RayOrigin.Y), static_cast<float>(RayOrigin.Z)},
		{static_cast<float>(Dir.X), static_cast<float>(Dir.Y), static_cast<float>(Dir.Z)},
		Targets.data(), Targets.size());
	if (Index < 0 || Index >= Actors.Num())
	{
		return nullptr;
	}
	// Re-derive the world hit point from the SAME numbers the rule used, so the point handed to
	// ApplyFleeFrom cannot disagree with the point that decided the hit.
	const float Depth = Targets[static_cast<size_t>(Index)].depth;
	const float S = (Depth - static_cast<float>(RayOrigin.X)) / static_cast<float>(Dir.X);
	OutHit = FVector(Depth, RayOrigin.Y + Dir.Y * S, RayOrigin.Z + Dir.Z * S);
	return Actors[Index];
}
```

- [ ] **Step 4 (GREEN): 빌드 두 번 + Automation**

```bash
cd /Users/hans/dev/aquarium
for i in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -2; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: `54`.

- [ ] **Step 5: 물리는지 확인한다 (변이 검사)** — `Targets.push_back(P->AsClickTarget());`을 잠시 `aquarium::ClickTarget T = P->AsClickTarget(); T.halfWidth *= 100.f; T.halfHeight *= 100.f; Targets.push_back(T);` 로 바꾸고 Automation을 돌린다. 기대: `Aquarium.Fish.ClickOnEmptyWaterHitsNothing` **실패**(3 m 위도 적중). 되돌린다.

- [ ] **Step 6: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.h unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.cpp unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp && git commit -m "$(cat <<'EOF'
feat(unreal): 클릭 대상 선택을 물고기 등록부에서 파생 (F-09)

적중 판정 규칙 자체는 aquarium::PickFrontmostHit에 있고, 서브시스템은
Neighbors()가 이웃 목록을 공급하듯 대상 목록만 공급한다. 반폭·반높이는
액터의 실제 바운드에서 파생하므로 종별 반경 표를 C++로 베끼지 않는다.
Automation 52 -> 54.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 6: 마우스 입력 연결과 반복 클릭 (F-09 · F-11)

**Files:** Modify `unreal/Aquarium/Source/Aquarium/DiverPlayerController.h`, Modify `unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp`, Modify `unreal/Aquarium/Source/Aquarium/HudWidget.h`, Modify `unreal/Aquarium/Source/Aquarium/HudWidget.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/ControllerTests.cpp`

- [ ] **Step 1 (RED): 테스트 2개를 먼저 쓴다**

`Tests/ControllerTests.cpp` 끝에 덧붙인다. 상단 include에 `#include "FishActor.h"`, `#include "FishSchoolSubsystem.h"`, `#include "aquarium/Flee.h"` 를 더한다.

```cpp
// F-11: mashing. Same fish mid-flee is ignored; same fish during recovery restarts the flee;
// a different fish is independent.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerRepeatedClicks, "Aquarium.Controller.RepeatedClicksFollowFleeRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControllerRepeatedClicks::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* A = World->SpawnActor<AFishActor>();
	AFishActor* B = World->SpawnActor<AFishActor>();
	A->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	B->PlaneOrigin = FVector(400.f, 200.f, 100.f);
	A->InitializeSwim();
	B->InitializeSwim();

	// First click on A, from its screen-left: flee goes right.
	A->ApplyFleeFrom(A->GetActorLocation() - FVector(0.f, 20.f, 0.f));
	const FVector FirstDirProbe = A->GetActorLocation();
	for (int32 i = 0; i < 12; ++i) { A->StepSwim(1.f / 60.f); }   // 0.2 s in
	const bool bMovingRight = A->GetActorLocation().Y > FirstDirProbe.Y;
	TestTrue(TEXT("first flee goes right"), bMovingRight);

	// Re-click A from the OTHER side while still fleeing: must be IGNORED.
	A->ApplyFleeFrom(A->GetActorLocation() + FVector(0.f, 20.f, 0.f));
	const double YBefore = A->GetActorLocation().Y;
	for (int32 i = 0; i < 12; ++i) { A->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("mid-flee re-click is ignored: still going right"), A->GetActorLocation().Y > YBefore);

	// Click B while A is fleeing: independent.
	TestTrue(TEXT("B untouched so far"), B->FleeState() == aquarium::BehaviorState::Normal);
	B->ApplyFleeFrom(B->GetActorLocation() - FVector(0.f, 20.f, 0.f));
	TestTrue(TEXT("B flees"), B->FleeState() == aquarium::BehaviorState::Fleeing);
	TestTrue(TEXT("A still fleeing on its own timer"), A->FleeState() == aquarium::BehaviorState::Fleeing);

	// Run A into recovery, then re-click from the other side: must RESTART the flee.
	for (int32 i = 0; i < 36; ++i) { A->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("A recovering"), A->FleeState() == aquarium::BehaviorState::Recovering);
	A->ApplyFleeFrom(A->GetActorLocation() + FVector(0.f, 20.f, 0.f));
	TestTrue(TEXT("recovery re-click restarts the flee"), A->FleeState() == aquarium::BehaviorState::Fleeing);
	const double YRestart = A->GetActorLocation().Y;
	for (int32 i = 0; i < 24; ++i) { A->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("and it now goes the other way"), A->GetActorLocation().Y < YRestart);
	return true;
}

// F-09: a click with no active session (the entry screen is up) disturbs nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerClickNeedsSession, "Aquarium.Controller.ClickIgnoredWithoutSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControllerClickNeedsSession::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	AFishActor* F = World->SpawnActor<AFishActor>();
	F->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	F->InitializeSwim();
	School->Register(F);

	ADiverPlayerController* PC = World->SpawnActor<ADiverPlayerController>();
	// No game mode session was ever begun, so the entry screen would be up.
	const bool bHandled = PC->HandleClickRay(FVector(0.f, 0.f, 100.f), FVector(1.f, 0.f, 0.f));
	TestFalse(TEXT("click is not handled without a session"), bHandled);
	TestTrue(TEXT("no fish was disturbed"), F->FleeState() == aquarium::BehaviorState::Normal);
	return true;
}
```

```bash
cd /Users/hans/dev/aquarium && "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -5
```
기대: **컴파일 실패** — `no member named 'HandleClickRay'`.

- [ ] **Step 2 (GREEN): `DiverPlayerController.h`**

`public:` 구역에 더한다.

```cpp
	// F-09. Applies one click, given as a world ray, to the frontmost fish it passes through.
	// Returns true when a fish was actually startled. Public so automation can drive the whole
	// click path without a viewport: -nullrhi has no game viewport, so deprojection (the ONLY
	// step this skips) cannot be exercised headlessly and is covered by the click-log CSV of a
	// real run instead.
	bool HandleClickRay(const FVector& RayOrigin, const FVector& RayDir);
```

`private:` 구역에 더한다.

```cpp
	// Bound to the left mouse button (IE_Pressed only -- see below). Reads the cursor, deprojects
	// it once, and forwards to HandleClickRay.
	void HandleClick();
	// One click at a viewport position in PIXELS. Both the real mouse and the dev-only scripted
	// clicks go through here, so the capture path and the play path are the same code.
	bool HandleClickAt(const FVector2D& ViewportPos);
```

- [ ] **Step 3 (GREEN): `DiverPlayerController.cpp`**

include에 `#include "FishSchoolSubsystem.h"` 를 더한다.

`SetupInputComponent()`의 `BindKey` 목록 끝에 한 줄을 더한다.

```cpp
		// F-09. IE_Pressed ONLY. macOS delivers BOTH IE_Pressed and IE_DoubleClick for the second
		// click of a fast double click, so binding the double click as well would make one
		// physical click of a mashing child count twice.
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADiverPlayerController::HandleClick);
```

`ApplyInputToPlayerFish` 아래에 세 함수를 더한다.

```cpp
void ADiverPlayerController::HandleClick()
{
	// The HUD exit button sits on top of the scene; a click that the button is taking must not
	// also startle whatever fish happens to be behind it. Slate handles the button itself, but
	// under FInputModeGameAndUI the key still reaches us, so this guard is ours to make.
	if (Hud && Hud->IsPointerOverExitButton())
	{
		return;
	}
	float X = 0.f, Y = 0.f;
	if (!GetMousePosition(X, Y))
	{
		return;
	}
	HandleClickAt(FVector2D(X, Y));
}

bool ADiverPlayerController::HandleClickAt(const FVector2D& ViewportPos)
{
	// The ONE place the engine's projection maths is used. Writing a closed-form screen -> plane
	// formula here would mean copying the FOV, aspect and near plane into a second place, which
	// is the duplicated-rule trap that hid the prop lane bug until M4b.
	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDir = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(static_cast<float>(ViewportPos.X), static_cast<float>(ViewportPos.Y),
	                                    WorldOrigin, WorldDir))
	{
		return false;
	}
	return HandleClickRay(WorldOrigin, WorldDir);
}

bool ADiverPlayerController::HandleClickRay(const FVector& RayOrigin, const FVector& RayDir)
{
	AAquariumGameMode* GM = GameMode();
	if (GM == nullptr || !GM->HasActiveSession())
	{
		return false;   // the entry screen is up; clicking the nickname box startles nobody
	}
	UWorld* W = GetWorld();
	UFishSchoolSubsystem* School = W ? W->GetSubsystem<UFishSchoolSubsystem>() : nullptr;
	if (School == nullptr)
	{
		return false;
	}
	FVector Hit = FVector::ZeroVector;
	AFishActor* Fish = School->PickFrontmostHit(RayOrigin, RayDir, Hit);
	if (Fish == nullptr)
	{
		return false;   // F-09: empty water affects nothing
	}
	Fish->ApplyFleeFrom(Hit);   // exactly one fish per click
	return true;
}
```

- [ ] **Step 4 (GREEN): HUD 호버 가드**

`HudWidget.h`의 `public:` 구역에 더한다.

```cpp
	// True while the cursor is over the exit button. Used by the click handler so a click the
	// button is taking does not also startle a fish behind it (F-09).
	bool IsPointerOverExitButton() const;
```

`HudWidget.cpp`에 더한다.

```cpp
bool UHudWidget::IsPointerOverExitButton() const
{
	return ExitButton != nullptr && ExitButton->IsHovered();
}
```

> **한계를 그대로 적는다**: 헤드리스에서는 호버가 발생하지 않으므로 이 가드는 Automation으로 검증되지 않는다. 클립에서 나가기 버튼을 눌러 물고기가 놀라지 않는지 확인한다(Task 10의 검토 항목).

- [ ] **Step 5: 빌드 두 번 + Automation**

```bash
cd /Users/hans/dev/aquarium
for i in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -2; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: `56`.

- [ ] **Step 6: 물리는지 확인한다 (변이 검사)** — `HandleClickRay`의 세션 가드를 잠시 지운다. 기대: `Aquarium.Controller.ClickIgnoredWithoutSession` **실패**. 되돌린다.

- [ ] **Step 7: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/DiverPlayerController.h unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp unreal/Aquarium/Source/Aquarium/HudWidget.h unreal/Aquarium/Source/Aquarium/HudWidget.cpp unreal/Aquarium/Source/Aquarium/Tests/ControllerTests.cpp && git commit -m "$(cat <<'EOF'
feat(unreal): 마우스 클릭을 BindKey로 연결 (F-09, F-11)

M3이 확인한 대로 Enhanced Input이 아니라 BindKey를 쓴다. IE_DoubleClick은
일부러 바인딩하지 않는다 - macOS가 빠른 두 번째 클릭에 IE_Pressed와
IE_DoubleClick을 둘 다 보내므로 연타가 두 번 처리된다. 입장 화면과 HUD
나가기 버튼에는 가드를 뒀다. Automation 54 -> 56.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 7: F-13 — 도망이 만드는 애니메이션 결함 검사

**Files:** Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

기존 `FacingIsContinuous` / `BoneAnglesAreContinuous` / `FacingHasNoLongTwistAcrossVertical`은
**도망을 한 번도 구동한 적이 없다.** `UpVectorStaysUpright`는 구조상 빨간불이 될 수 없으므로
같은 모양의 테스트를 새로 만들지 않는다. 여기서 더하는 셋은 전부 도망이 새로 만드는 결함을 본다.

- [ ] **Step 1 (RED): 테스트 3개를 먼저 쓴다**

```cpp
// F-13: the flee flips the desired direction by up to 180 degrees in ONE frame -- the harshest
// direction input in this game. The facing must still be rate limited. The limit is DERIVED from
// aquarium::FacingParams, never written as a literal.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishFacingContinuousAcrossFlee, "Aquarium.Fish.FacingIsContinuousAcrossFlee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishFacingContinuousAcrossFlee::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	Fish->bPlayerControlled = true;
	Fish->InitializeSwim();
	Fish->SetInputDirection(FVector2D(1.f, 0.f));                 // swimming screen-right
	for (int32 i = 0; i < 60; ++i) { Fish->StepSwim(1.f / 60.f); }

	const float Dt = 1.f / 60.f;
	aquarium::FacingParams FP;
	FP.maxTurnRateDegPerSec = Fish->MaxFacingTurnRate;
	FP.uprightRollRateDegPerSec = Fish->MaxFacingTurnRate;
	FP.steepRollRateDegPerSec = Fish->SteepRollRate;
	FP.steepBeginSin = Fish->SteepBeginSin;
	// Worst case allowance for one step: full swing plus full twist, both from the rules layer.
	const float Allowed = aquarium::MaxSwingStepDeg(FP, Dt) + aquarium::MaxTwistStepDeg(1.f, FP, Dt);

	// Click directly in FRONT of the fish: the flee direction is the exact reverse of its heading.
	Fish->ApplyFleeFrom(Fish->GetActorLocation() + FVector(0.f, 10.f, 0.f));
	FQuat Prev = Fish->GetActorQuat();
	float Worst = 0.f;
	for (int32 i = 0; i < 180; ++i)
	{
		Fish->StepSwim(Dt);
		const FQuat Now = Fish->GetActorQuat();
		const float StepDeg = FMath::RadiansToDegrees(Prev.AngularDistance(Now));
		Worst = FMath::Max(Worst, StepDeg);
		Prev = Now;
	}
	TestTrue(FString::Printf(TEXT("worst per-frame facing change %.1f deg <= %.1f deg"), Worst, Allowed),
		Worst <= Allowed + 1.f);
	return true;
}

// F-13: "does not slide" -- the body must point where it is going, throughout flee and recovery.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishFleeDoesNotSlide, "Aquarium.Fish.FleeDoesNotSlide",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishFleeDoesNotSlide::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	Fish->InitializeSwim();
	for (int32 i = 0; i < 60; ++i) { Fish->StepSwim(1.f / 60.f); }
	Fish->ApplyFleeFrom(Fish->GetActorLocation() + FVector(0.f, 10.f, 0.f));

	FVector Prev = Fish->GetActorLocation();
	float WorstDeg = 0.f;
	int32 Samples = 0;
	for (int32 i = 0; i < 180; ++i)
	{
		Fish->StepSwim(1.f / 60.f);
		const FVector Now = Fish->GetActorLocation();
		const FVector Step = Now - Prev;
		Prev = Now;
		// Skip the turnaround itself: while the velocity passes near zero there is no meaningful
		// travel direction. 0.05 cm per frame at 60 fps is 3 cm/s, well under any cruise speed.
		if (Step.Size() < 0.05) { continue; }
		const float Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			static_cast<float>(FVector::DotProduct(Step.GetSafeNormal(), Fish->GetActorForwardVector())), -1.f, 1.f)));
		WorstDeg = FMath::Max(WorstDeg, Deg);
		++Samples;
	}
	TestTrue(TEXT("enough samples to mean anything"), Samples > 120);
	// The facing is a rate-limited slew toward the velocity, so a bounded lag is expected; a fish
	// travelling sideways or backwards is not.
	TestTrue(FString::Printf(TEXT("worst travel-vs-forward angle %.1f deg <= 60 deg"), WorstDeg),
		WorstDeg <= 60.f);
	return true;
}

// F-13: "amplitude and period follow speed AND state". The flee speed burst must show in the tail.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishFleeIncreasesTailAmplitude, "Aquarium.Fish.FleeIncreasesTailAmplitude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishFleeIncreasesTailAmplitude::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	Fish->InitializeSwim();
	for (int32 i = 0; i < 120; ++i) { Fish->StepSwim(1.f / 60.f); }
	const float CruiseSpeed = Fish->CurrentSpeed();
	Fish->ApplyFleeFrom(Fish->GetActorLocation() + FVector(0.f, 10.f, 0.f));
	for (int32 i = 0; i < 30; ++i) { Fish->StepSwim(1.f / 60.f); }
	const float FleeSpeed = Fish->CurrentSpeed();

	aquarium::SwimAnimParams AP;
	AP.boneCount = 7;
	// Expectation derived from the rules layer, not from a literal amplitude.
	const float CruiseAmp = aquarium::SwimAnimation::Amplitude(CruiseSpeed, AP);
	const float FleeAmp = aquarium::SwimAnimation::Amplitude(FleeSpeed, AP);
	TestTrue(FString::Printf(TEXT("flee is faster (%.1f > %.1f)"), FleeSpeed, CruiseSpeed),
		FleeSpeed > CruiseSpeed * 1.2f);
	TestTrue(FString::Printf(TEXT("so the tail swings wider (%.2f > %.2f)"), FleeAmp, CruiseAmp),
		FleeAmp > CruiseAmp);
	return true;
}
```

```bash
cd /Users/hans/dev/aquarium
for i in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -2; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: `59`. **여기서 실패가 나오면 그것은 M5가 만든 진짜 결함이다** — 테스트를 느슨하게 만들지 말고 `Flee.h`의 `fleeSpeedScale`이나 규칙 순서를 고친다.

- [ ] **Step 2: 세 테스트 전부 물리는지 확인한다 (변이 검사 3회)**

변이 1 — `FishActor.cpp`에서 slew를 끄고 `SetActorRotation(Target)`을 직접 쓴다. 기대: `FacingIsContinuousAcrossFlee` **실패**.
변이 2 — `fleeSpeedScale`을 `1.0f`로 바꾼다. 기대: `FleeIncreasesTailAmplitude` **실패**.
변이 3 — 도망 층에서 속도 배율 줄만 남기고 `Desired = Flee.FleeDirection();`을 `Desired = Flee.FleeDirection() * -1.f;`의 **수직 벡터**(`{-Flee.FleeDirection().y, Flee.FleeDirection().x}`)로 바꾼다(옆으로 미끄러지게 만든다). 기대: `FleeDoesNotSlide` **실패**.

각 변이 뒤에 반드시 되돌리고 `59`를 재확인한다. **어떤 변이도 빨간불을 만들지 못하면 그 테스트는 아무것도 검사하지 않는 것이다** — 빨간불이 켜지는 변이를 찾거나, 못 찾았다는 사실을 후속 항목에 적는다(`UpVectorStaysUpright`가 바로 그 사례다).

- [ ] **Step 3: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp && git commit -m "$(cat <<'EOF'
test(unreal): 도망이 만드는 애니메이션 결함 검사 추가 (F-13)

기존 연속성 테스트는 도망을 한 번도 구동한 적이 없다. 코앞을 클릭해
원하는 방향을 한 프레임에 180도 뒤집는 최악의 경우에서 방향 연속성,
미끄러짐 없음, 진폭이 상태를 따르는 것을 고정한다. 한도와 기대값은
FacingParams와 SwimAnimation에서 유도한다. Automation 56 -> 59.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 8: 개발 전용 플래그 — `-AquariumAutoClick` / `-AquariumClickLog`

**Files:** Modify `unreal/Aquarium/Source/Aquarium/DiverPlayerController.h`, Modify `unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/ControllerTests.cpp`

**문법을 정확히 지킨다.** 토큰 = `<초>@<nx>x<ny>`. 구분자는 `@` 하나와 `x` 하나. 콜론 없음.

- [ ] **Step 1 (RED): 파서 테스트를 먼저 쓴다**

`Tests/ControllerTests.cpp` 끝에 덧붙인다.

```cpp
// Dev-only scripted clicks. The grammar is pinned here because the -AquariumAutoInput parser
// taught this project that a silently-ignored bad token produces a convincing but empty capture.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerAutoClickParses, "Aquarium.Controller.AutoClickPatternParses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControllerAutoClickParses::RunTest(const FString&)
{
	FString Pattern;
	TestTrue(TEXT("flag is found"), ADiverPlayerController::ParseAutoClick(
		TEXT("-AquariumAutoClick=4.0@0.50x0.46,7.25@0.12x0.80"), Pattern));
	TestEqual(TEXT("pattern survives the commas"), Pattern, FString(TEXT("4.0@0.50x0.46,7.25@0.12x0.80")));
	TestFalse(TEXT("absent flag"), ADiverPlayerController::ParseAutoClick(TEXT("-Other=1"), Pattern));

	const TArray<ADiverPlayerController::FAutoClick> Good =
		ADiverPlayerController::BuildAutoClicks(TEXT("4.0@0.50x0.46, 7.25@0.12x0.80"));
	TestEqual(TEXT("two clicks"), Good.Num(), 2);
	TestEqual(TEXT("first time"), Good[0].TimeSeconds, 4.0f, 1e-3f);
	TestEqual(TEXT("first nx"), Good[0].Normalized.X, 0.50f, 1e-3f);
	TestEqual(TEXT("first ny"), Good[0].Normalized.Y, 0.46f, 1e-3f);
	TestEqual(TEXT("second time"), Good[1].TimeSeconds, 7.25f, 1e-3f);

	// Every one of these is the kind of token an author invents from memory. All must be dropped.
	const TArray<ADiverPlayerController::FAutoClick> Bad = ADiverPlayerController::BuildAutoClicks(
		TEXT("4.0:0.5x0.5,4.0@0.5,4.0@0.5x0.5x0.5,@0.5x0.5,4.0@1.5x0.5,4.0@-0.1x0.5,-1@0.5x0.5,abc@0.5x0.5"));
	TestEqual(TEXT("every malformed token is dropped"), Bad.Num(), 0);
	return true;
}
```

```bash
cd /Users/hans/dev/aquarium && "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -5
```
기대: **컴파일 실패** — `no member named 'ParseAutoClick'` / `'FAutoClick'`.

- [ ] **Step 2 (GREEN): `DiverPlayerController.h`**

`public:` 구역에 더한다(`FAutoInputStep`이 private인 것과 달리, 파서 테스트가 닿아야 하므로 public).

```cpp
	// One scripted click: when, and where in the viewport (0..1 of width/height).
	struct FAutoClick
	{
		float TimeSeconds = 0.f;
		FVector2D Normalized = FVector2D::ZeroVector;
	};
	// Parses -AquariumAutoClick=<pattern>; false when absent or empty.
	static bool ParseAutoClick(const TCHAR* CmdLine, FString& OutPattern);
	// Parses "<sec>@<nx>x<ny>[,<sec>@<nx>x<ny>...]". Separators are exactly one '@' and one 'x';
	// there is NO colon form. Malformed or out-of-range tokens are WARNED ABOUT AND DROPPED, so
	// every capture harness must assert that the warning count is zero and that the "armed N"
	// count matches what it asked for.
	static TArray<FAutoClick> BuildAutoClicks(const FString& Pattern);
	// Parses -AquariumClickLog=<absolute csv path>; false when absent or empty.
	static bool ParseClickLogPath(const TCHAR* CmdLine, FString& OutPath);
```

`private:` 구역에 더한다.

```cpp
	TArray<FAutoClick> AutoClicks;
	int32 NextAutoClick = 0;
	float AutoClickElapsed = 0.f;
	void StartAutoClickIfRequested();
	void AdvanceAutoClick(float DeltaSeconds);
	// Dev-only click record: one row per click attempt, real or scripted. Never a nickname.
	FString ClickLogPath;
	TArray<FString> ClickLogRows;
	void StartClickLogIfRequested();
	void WriteClickLog();
```

- [ ] **Step 3 (GREEN): `DiverPlayerController.cpp`**

파일 끝에 더한다.

```cpp
bool ADiverPlayerController::ParseAutoClick(const TCHAR* CmdLine, FString& OutPattern)
{
	OutPattern.Reset();
	// bShouldStopOnSeparator=false so an unquoted comma-separated pattern survives intact,
	// exactly as ParseAutoInput does.
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumAutoClick="), OutPattern, /*bShouldStopOnSeparator*/ false))
	{
		return false;
	}
	OutPattern.TrimStartAndEndInline();
	return !OutPattern.IsEmpty();
}

TArray<ADiverPlayerController::FAutoClick> ADiverPlayerController::BuildAutoClicks(const FString& Pattern)
{
	TArray<FAutoClick> Clicks;
	TArray<FString> Tokens;
	Pattern.ParseIntoArray(Tokens, TEXT(","), /*CullEmpty*/ true);
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}
		FString TimePart, CoordPart, XPart, YPart;
		const bool bSplitAt = Token.Split(TEXT("@"), &TimePart, &CoordPart);
		const bool bSplitX = bSplitAt && CoordPart.Split(TEXT("x"), &XPart, &YPart);
		if (!bSplitX)
		{
			// The token is dev test data, so echoing it is safe; it never carries a nickname.
			UE_LOG(LogTemp, Warning, TEXT("AquariumAutoClick: bad token '%s'; entry ignored"), *Token);
			continue;
		}
		TimePart.TrimStartAndEndInline();
		XPart.TrimStartAndEndInline();
		YPart.TrimStartAndEndInline();
		if (!TimePart.IsNumeric() || !XPart.IsNumeric() || !YPart.IsNumeric() || YPart.Contains(TEXT("x")))
		{
			UE_LOG(LogTemp, Warning, TEXT("AquariumAutoClick: bad token '%s'; entry ignored"), *Token);
			continue;
		}
		const float T = FCString::Atof(*TimePart);
		const float Nx = FCString::Atof(*XPart);
		const float Ny = FCString::Atof(*YPart);
		if (!(T > 0.f) || Nx < 0.f || Nx > 1.f || Ny < 0.f || Ny > 1.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("AquariumAutoClick: coords out of range in '%s'; entry ignored"), *Token);
			continue;
		}
		FAutoClick C;
		C.TimeSeconds = T;
		C.Normalized = FVector2D(Nx, Ny);
		Clicks.Add(C);
	}
	// Fire in time order whatever order they were written in.
	Clicks.Sort([](const FAutoClick& A, const FAutoClick& B) { return A.TimeSeconds < B.TimeSeconds; });
	return Clicks;
}

void ADiverPlayerController::StartAutoClickIfRequested()
{
#if !UE_BUILD_SHIPPING
	AutoClicks.Reset();
	NextAutoClick = 0;
	AutoClickElapsed = 0.f;
	FString Pattern;
	if (!ParseAutoClick(FCommandLine::Get(), Pattern))
	{
		return;
	}
	AutoClicks = BuildAutoClicks(Pattern);
	// ALWAYS logged, even for 0: a harness must be able to assert the armed count rather than
	// discovering after the fact that its whole script was dropped by the parser.
	UE_LOG(LogTemp, Warning, TEXT("AquariumAutoClick: armed %d clicks"), AutoClicks.Num());
#endif
}

void ADiverPlayerController::AdvanceAutoClick(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	if (NextAutoClick >= AutoClicks.Num())
	{
		return;
	}
	AutoClickElapsed += DeltaSeconds;
	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	if (ViewportSize.X <= 0.f || ViewportSize.Y <= 0.f)
	{
		return;
	}
	while (NextAutoClick < AutoClicks.Num() && AutoClicks[NextAutoClick].TimeSeconds <= AutoClickElapsed)
	{
		const FAutoClick& C = AutoClicks[NextAutoClick++];
		// Same entry point as a real mouse click: one code path, so the capture verifies the
		// thing the child will actually use.
		HandleClickAt(FVector2D(C.Normalized.X * ViewportSize.X, C.Normalized.Y * ViewportSize.Y));
	}
#endif
}

bool ADiverPlayerController::ParseClickLogPath(const TCHAR* CmdLine, FString& OutPath)
{
	OutPath.Reset();
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumClickLog="), OutPath))
	{
		return false;
	}
	OutPath.TrimStartAndEndInline();
	return !OutPath.IsEmpty();
}

void ADiverPlayerController::StartClickLogIfRequested()
{
#if !UE_BUILD_SHIPPING
	FString Path;
	if (!ParseClickLogPath(FCommandLine::Get(), Path))
	{
		return;
	}
	ClickLogPath = Path;
	ClickLogRows.Reset();
	ClickLogRows.Add(TEXT("index,time_s,ndc_x,ndc_y,hit_plane_x,hit,state_before"));
#endif
}

void ADiverPlayerController::WriteClickLog()
{
#if !UE_BUILD_SHIPPING
	if (ClickLogPath.IsEmpty())
	{
		return;
	}
	FString Csv;
	for (const FString& Row : ClickLogRows) { Csv += Row + TEXT("\n"); }
	if (FFileHelper::SaveStringToFile(Csv, *ClickLogPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AquariumClickLog: wrote %d clicks to %s"),
			ClickLogRows.Num() - 1, *ClickLogPath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AquariumClickLog: cannot write %s"), *ClickLogPath);
	}
	ClickLogPath.Reset();
	ClickLogRows.Empty();
#endif
}
```

`HandleClickAt`의 끝을 기록하도록 바꾼다(반환 직전).

```cpp
	const bool bHandled = HandleClickRay(WorldOrigin, WorldDir);
#if !UE_BUILD_SHIPPING
	if (!ClickLogPath.IsEmpty())
	{
		FVector2D Size = FVector2D(1.f, 1.f);
		if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->GetViewportSize(Size); }
		ClickLogRows.Add(FString::Printf(TEXT("%d,%.3f,%.4f,%.4f,%.1f,%d,%s"),
			ClickLogRows.Num() - 1, AutoClickElapsed,
			ViewportPos.X / FMath::Max(Size.X, 1.f), ViewportPos.Y / FMath::Max(Size.Y, 1.f),
			LastClickPlaneX, bHandled ? 1 : 0, TEXT("Normal")));
	}
#endif
	return bHandled;
```

`LastClickPlaneX`는 `HandleClickRay`가 채우는 `float LastClickPlaneX = 0.f;` private 멤버다
(`Hit.X`를 대입하고, 빈 바다면 0). `state_before` 열은 맞은 물고기의 클릭 직전 상태 문자열이며
`HandleClickRay`가 `LastClickState`(FString private 멤버)에 `TEXT("Normal")`/`TEXT("Fleeing")`/
`TEXT("Recovering")`를 채워 두고 위에서 그것을 쓴다. **별명은 어떤 열에도 들어가지 않는다.**

`BeginPlay()`의 초기화 호출 목록에 두 줄을 더한다.

```cpp
	StartAutoClickIfRequested();
	StartClickLogIfRequested();
```

`Tick()`의 `AdvanceAutoInput(DeltaSeconds);` 바로 아래(같은 `#if !UE_BUILD_SHIPPING` 안)에 더한다.

```cpp
	AdvanceAutoClick(DeltaSeconds);
```

`EndPlay()`의 `WriteFrameStats();` 옆에 더한다.

```cpp
	WriteClickLog();
```

include에 `#include "Engine/GameViewportClient.h"` 와 `#include "Engine/Engine.h"` 를 더한다.

- [ ] **Step 4 (GREEN): 빌드 두 번 + Automation**

```bash
cd /Users/hans/dev/aquarium
for i in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | tail -2; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: `60`.

> **개수 확인 2**: 3(Task 3) + 2(Task 4) + 2(Task 5) + 2(Task 6) + 3(Task 7) + 1(Task 8) = **13개**를 더해 Automation은 47 → **60개**다. 뒤의 모든 기대 문자열과 문서 갱신은 60을 쓴다.

- [ ] **Step 5: 물리는지 확인한다 (변이 검사)** — `BuildAutoClicks`의 범위 검사(`Nx < 0.f || Nx > 1.f ...`)를 잠시 지운다. 기대: `Aquarium.Controller.AutoClickPatternParses` **실패**(`1.5@` 토큰이 살아남아 `Bad.Num()`이 0이 아니다). 되돌린다.

- [ ] **Step 6: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/DiverPlayerController.h unreal/Aquarium/Source/Aquarium/DiverPlayerController.cpp unreal/Aquarium/Source/Aquarium/Tests/ControllerTests.cpp && git commit -m "$(cat <<'EOF'
feat(unreal): 개발 전용 예약 클릭과 클릭 로그 추가

-AquariumAutoClick=<초>@<nx>x<ny>[,...] 로 클릭을 예약하고,
-AquariumClickLog=<csv> 로 클릭별 적중 결과를 남긴다. 스크립트 클릭도
실제 마우스와 같은 HandleClickAt을 통과하므로 캡처가 검증하는 것이
아이가 실제로 쓰는 경로다. 파서는 잘못된 토큰을 경고 후 버리므로
armed 개수를 반드시 로그에 찍는다. Automation 59 -> 60.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 9: 캡처 하네스 — `scripts/render_m5_click.sh`

**Files:** Create `scripts/render_m5_click.sh`

- [ ] **Step 1: 스크립트를 만든다**

```bash
cd /Users/hans/dev/aquarium && cat > scripts/render_m5_click.sh <<'SH'
#!/bin/bash
# M5 review artefacts. Fleeing is entirely a TIME-AXIS phenomenon: a still cannot
# tell a fleeing fish from a swimming one. The CLIP is the artefact that gets
# judged; the still only proves M4c's look did not regress.
#
# Outputs (docs/reviews/<date>-m5-*):
#   click.mp4   30 fps, 30 s, same map/seed/camera as the M4c clip, with scripted
#               clicks: a background fish, empty water, the same fish twice fast,
#               and the player's own fish while a key is held.
#   clicks.csv  one row per click attempt (what was aimed at, what was hit)
#   flee.png    t=12 frame of the clip
#   compare.png <before=M4c scene still> | flee.png (hstack)
#
# AUTO-CLICK TOKEN FORMAT -- read from ADiverPlayerController::BuildAutoClicks,
# not from memory. Tokens are comma separated; each is
#     <seconds>@<nx>x<ny>
# with EXACTLY one '@' and one 'x'. There is NO colon form. nx/ny are viewport
# fractions in 0..1. The parser only WARNS on junk, so a typo silently removes a
# click and still produces a convincing clip -- which is why this script asserts
# both warning counts are 0 AND that "armed N" matches CLICK_COUNT below.
#
# AUTO-INPUT TOKEN FORMAT: <direction-letter><seconds>, letters R/L/U/D/0, no
# colons, no diagonals. Same parser caveat, same assertion.
#
# The nickname passed via -AquariumAutoNickname is TEST DATA ONLY: the engine
# echoes the whole command line into its log.
#
# Notes (verified on UE 5.8.2 / macOS), same as render_m4c_compare.sh:
#   - Builds the editor target twice first, then uses the EDITOR binary.
#   - -benchmark -fps=N fixes the timestep, -seconds=N exits by itself.
#   - -ForceRes is required or GameUserSettings.ini overrides the resolution.
#   - Frames land in Saved/UiFrames/UiFrame%05d.png.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
UE="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
FFMPEG="${FFMPEG:-/opt/homebrew/bin/ffmpeg}"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
FRAMES="$ROOT/unreal/Aquarium/Saved/UiFrames"
REVIEWS="$ROOT/docs/reviews"
LOG="$HOME/Library/Logs/Aquarium/Aquarium.log"
MAP="${MAP:-ReefM1}"
FPS="${FPS:-30}"
SECONDS_TO_RUN="${SECONDS_TO_RUN:-30}"
AUTO_NICKNAME="${AUTO_NICKNAME:-니모}"
ASSIGNMENT_SEED="${ASSIGNMENT_SEED:-1}"
SKIP_FRAMES="${SKIP_FRAMES:-15}"
DATE="$(date +%F)"
# The session starts ~2 s after BeginPlay (kAutoSubmitDelay), so every click is
# scheduled after that. Aiming points are viewport fractions.
#   6.0  a background fish, upper left        7.0  the SAME fish 1 s later (mid-flee -> ignored)
#   9.0  empty water, top of frame            12.0 a background fish, right
#   16.0 the player's own fish (centre)       16.6 the same fish again (mid-flee -> ignored)
#   19.0 the player's own fish (recovery -> restarts)
AUTO_CLICK="${AUTO_CLICK:-6.0@0.30x0.40,7.0@0.30x0.40,9.0@0.50x0.10,12.0@0.72x0.55,16.0@0.50x0.55,16.6@0.50x0.55,19.0@0.50x0.55}"
CLICK_COUNT="${CLICK_COUNT:-7}"
# A key held throughout, so the F-12 "held key resumes at recovery" moment is in
# the clip at t=16..19.
AUTO_INPUT="${AUTO_INPUT:-R3,0 2,L3,0 2,R3,U2,L3,D2,0 2,R6,0 2}"
BEFORE="${BEFORE:-$REVIEWS/2026-09-21-m4c-scene.png}"

case "$BEFORE" in
  *m5*) echo "BEFORE must not be an m5 artefact (M4b once compared M4b with M4b)"; exit 1;;
esac
[ -f "$BEFORE" ] || { echo "missing BEFORE still: $BEFORE"; exit 1; }

if [ "${SKIP_BUILD:-0}" != "1" ]; then
  for i in 1 2; do   # UBT writes UnrealEditor.modules one build late
    "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development \
      -Project="$PROJ" -WaitMutex
  done
fi

rm -rf "$FRAMES"; mkdir -p "$FRAMES" "$REVIEWS"
rm -f "$LOG"
CSV="$REVIEWS/$DATE-m5-clicks.csv"

"$UE" "$PROJ" "$MAP" -game -windowed -ResX=1920 -ResY=1080 -ForceRes \
  -benchmark -fps="$FPS" -seconds="$SECONDS_TO_RUN" -notexturestreaming \
  -unattended -nosplash -log \
  -AquariumAutoNickname="$AUTO_NICKNAME" \
  -AquariumAssignmentSeed="$ASSIGNMENT_SEED" \
  -AquariumAutoInput="$AUTO_INPUT" \
  -AquariumAutoClick="$AUTO_CLICK" \
  -AquariumClickLog="$CSV" \
  -AquariumCaptureUI="$FRAMES" || true

# --- assertions: a capture that verified nothing is worse than no capture ------
ARMED="$(grep -o 'AquariumAutoClick: armed [0-9]*' "$LOG" | tail -1 | awk '{print $3}')"
CLICK_BAD="$(grep -c 'AquariumAutoClick: bad token' "$LOG" || true)"
CLICK_RANGE="$(grep -c 'AquariumAutoClick: coords out of range' "$LOG" || true)"
INPUT_UNKNOWN="$(grep -c 'AquariumAutoInput: unknown direction' "$LOG" || true)"
INPUT_BAD="$(grep -c 'AquariumAutoInput: bad duration' "$LOG" || true)"
MATERIAL="$(grep -icE 'Failed to compile Material|Sampler type|Default Material|WorldGridMaterial' "$LOG" || true)"
echo "ARMED=$ARMED CLICK_BAD=$CLICK_BAD CLICK_RANGE=$CLICK_RANGE INPUT_UNKNOWN=$INPUT_UNKNOWN INPUT_BAD=$INPUT_BAD MATERIAL=$MATERIAL"
[ "$ARMED" = "$CLICK_COUNT" ] || { echo "auto-click script was silently truncated"; exit 1; }
[ "$CLICK_BAD" = "0" ] && [ "$CLICK_RANGE" = "0" ] || { echo "auto-click parse warnings"; exit 1; }
[ "$INPUT_UNKNOWN" = "0" ] && [ "$INPUT_BAD" = "0" ] || { echo "auto-input parse warnings"; exit 1; }
[ "$MATERIAL" = "0" ] || { echo "materials fell back to the grey default"; exit 1; }
[ -f "$CSV" ] || { echo "no click log written"; exit 1; }
ROWS="$(($(wc -l < "$CSV") - 1))"
HITS="$(awk -F, 'NR>1 && $6==1' "$CSV" | wc -l | tr -d ' ')"
echo "CLICK_ROWS=$ROWS CLICK_HITS=$HITS"
[ "$ROWS" = "$CLICK_COUNT" ] || { echo "not every scripted click reached HandleClickAt"; exit 1; }
# 9.0@0.50x0.10 aims at open water near the top of the frame, so at least one
# miss must be recorded. All hits would mean the ellipse test is too generous.
[ "$HITS" -ge 1 ] && [ "$HITS" -lt "$CLICK_COUNT" ] || { echo "click log is implausible: $HITS/$CLICK_COUNT hits"; exit 1; }

# --- artefacts ----------------------------------------------------------------
"$FFMPEG" -y -framerate "$FPS" -start_number "$SKIP_FRAMES" \
  -i "$FRAMES/UiFrame%05d.png" -c:v libx264 -pix_fmt yuv420p -crf 20 \
  "$REVIEWS/$DATE-m5-click.mp4"
STILL_INDEX=$((SKIP_FRAMES + FPS * 12))
cp "$(printf '%s/UiFrame%05d.png' "$FRAMES" "$STILL_INDEX")" "$REVIEWS/$DATE-m5-flee.png"
"$FFMPEG" -y -i "$BEFORE" -i "$REVIEWS/$DATE-m5-flee.png" \
  -filter_complex hstack "$REVIEWS/$DATE-m5-compare.png"
ls -l "$REVIEWS/$DATE-m5-"*
SH
chmod +x scripts/render_m5_click.sh
```

- [ ] **Step 2: 실행**

```bash
cd /Users/hans/dev/aquarium && ./scripts/render_m5_click.sh 2>&1 | tail -20
```
기대: `ARMED=7 CLICK_BAD=0 CLICK_RANGE=0 INPUT_UNKNOWN=0 INPUT_BAD=0 MATERIAL=0`, `CLICK_ROWS=7`,
`CLICK_HITS`가 1 이상 7 미만, 그리고 산출물 4개. **어느 단언이든 걸리면 캡처를 믿지 않는다.**

- [ ] **Step 3: 클릭 로그를 직접 읽는다**

```bash
cd /Users/hans/dev/aquarium && cat docs/reviews/$(date +%F)-m5-clicks.csv
```
확인할 것: (1) 7행, (2) `9.0` 근처 행의 `hit`가 0(빈 바다), (3) 적중 행의 `hit_plane_x`가 실제 평면 값(220 또는 330~700), (4) **별명이 어느 열에도 없다.**

- [ ] **Step 4: 클립을 직접 본다** — `docs/reviews/<날짜>-m5-click.mp4`. 확인할 것 다섯 가지를 기록한다.
  1. t≈6에서 물고기 한 마리가 **확 빨라지며** 클릭 지점 반대로 간다.
  2. t≈7의 두 번째 클릭에 **반응이 없다**(도망 중 무시).
  3. t≈9의 빈 바다 클릭에 **화면의 어떤 물고기도 반응하지 않는다.**
  4. t≈16에서 내 물고기가 방향키를 무시하고 도망하고, t≈16.8부터 **다시 눌린 키를 따른다.**
  5. **순간 반전도 미끄러짐도 없다**(F-13). 있으면 Task 7로 돌아간다.
  6. HUD 나가기 버튼 위를 클릭해도 뒤 물고기가 놀라지 않는다 — **이것은 수동 확인**이다(호버는 헤드리스에서 발생하지 않는다). 창 모드로 한 번 직접 눌러 본다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add scripts/render_m5_click.sh docs/reviews/$(date +%F)-m5-click.mp4 docs/reviews/$(date +%F)-m5-flee.png docs/reviews/$(date +%F)-m5-compare.png docs/reviews/$(date +%F)-m5-clicks.csv && git commit -m "$(cat <<'EOF'
chore: M5 클릭 캡처 하네스와 산출물

예약 클릭·자동 입력의 파싱 경고 0건, armed 개수 일치, 머티리얼 0건,
클릭 로그 행 수와 적중/비적중 혼재를 전부 단언한다. 단언이 없으면
"클릭이 하나도 발사되지 않은 그럴듯한 클립"을 구분할 수 없다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 10: 성능 재측정

**Files:** Create `docs/reviews/<날짜>-m5-perf.md`

기준선: M4c **평균 63.46 fps / p95 16.72 ms**(1920×1080 에디터 빌드). SRS 게이트 60 fps / 22 ms.
**실행 간 노이즈 바닥 약 1.3 fps. 1.3 fps 미만 차이는 어느 방향이든 신호로 읽지 않는다.**

설계상 예측은 **변화 없음**이다(클릭당 비용은 400 flop 남짓이고 매 프레임 화면 투영을 하지
않는다). M4b에서 "반투명 커튼이 비쌀 것"이라는 예측이 측정으로 반증된 적이 있으므로 그대로
믿지 않고 잰다.

- [ ] **Step 1: 기본 설정 2회**

```bash
cd /Users/hans/dev/aquarium && ./scripts/measure_m2b_perf.sh 2>&1 | tail -5
```
두 번 돌려 평균 fps가 서로 1.3 fps 안에 드는지 본다. 들지 않으면 배경 프로세스를 정리하고 다시 잰다.

- [ ] **Step 2: 항목별 토글**

```bash
cd /Users/hans/dev/aquarium && EXTRA_ARGS="-AquariumNoFlee" ./scripts/measure_m2b_perf.sh 2>&1 | tail -3
```

- [ ] **Step 3: 보고서 작성** — `docs/reviews/<날짜>-m5-perf.md`에 M4c 보고서와 같은 형식으로. 반드시 포함할 것:
  - 두 설정의 평균 fps / p95 ms 표
  - M4c 기준선(63.46 / 16.72) 대비 차이와 **1.3 fps 노이즈 바닥 위인지 아래인지 한 줄 판정**
  - 클릭당 비용 모델(역투영 1회 + 37회 타원 검사)과 틱당 비용(물고기당 비교 2회)
  - **`GridSizeZ` 128 → 64 레버를 쓰지 않았다는 사실과 그 이유**(안 썼다면)

- [ ] **Step 4: 판정**
  - 평균 ≥ 60 fps: 그대로 간다. **레버는 M6으로 남긴다.** 미리 쓰면 M6 쿡 빌드의 실제 여유가 영구히 가려진다.
  - 평균 < 60 fps: (1) `GridSizeZ` 128 → 64, (2) 부유 입자 커튼 3 → 1, (3) SSAO 끄기 순서로 **하나씩** 되돌리고 **되돌릴 때마다 재측정**한다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add docs/reviews/$(date +%F)-m5-perf.md docs/reviews/$(date +%F)-m2b-frametimes.csv && git commit -m "$(cat <<'EOF'
perf: M5 프레임 시간 재측정

기본 설정과 -AquariumNoFlee 두 설정으로 측정했다. 1.3 fps 노이즈 바닥을
기준으로 신호 여부를 판정한다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 11: 문서 갱신과 마무리

**Files:** Modify `docs/TASK.md`, Modify `docs/SETUP.md`, Modify this plan

- [ ] **Step 1: `docs/TASK.md` 갱신**
  - 로드맵 표의 M5 행을 M4a/M4b/M4c와 같은 형식의 구현 완료 기록으로 바꾼다: 규칙 **103** + Automation **60**, 측정한 평균 fps / p95, 산출물 경로, `**사용자 시각 검토: 대기**`.
  - "검증 현황"의 규칙 90 → 103, Automation 47 → 60으로 갱신하고 신규 테스트를 한 줄로 요약한다.
  - **"미구현 Unreal 연동: F-09 레이캐스트(M5)" 줄을 지우고, 실제 상태를 바로잡아 적는다** — M5 시작 시점에 없던 것은 레이캐스트뿐이 아니라 F-09~F-13의 엔진 연동 전부였고, 적중 판정은 레이캐스트가 아니라 **해석적 광선–평면 판정**으로 구현했다는 사실.
  - 상단 문단의 "남은 단계는 M5와 M6"을 "남은 단계는 M6"으로 갱신한다.

- [ ] **Step 2: `docs/SETUP.md` 갱신**
  - 개발 전용 플래그 목록에 `-AquariumAutoClick`, `-AquariumClickLog`, `-AquariumNoFlee`를 더한다.
  - `-AquariumAutoInput` 문법 절 바로 아래에 **`-AquariumAutoClick` 토큰 문법 절**을 같은 형식으로 새로 쓴다: `<초>@<nx>x<ny>`, `@`와 `x` 각각 하나, 콜론 없음, 경고 문자열 두 개, `armed N` 로그, 하네스가 단언한다는 사실.
  - 캡처 스크립트 목록에 `render_m5_click.sh`를 더한다.

- [ ] **Step 3: 이 계획의 "구현 중 발견한 후속 항목" 절을 채운다** — 최소한 다음은 반드시 기록한다.
  - Task 4 Step 1에서 `FleeBeatsArrowKeys`가 **처음부터 통과했는지**(Task 3의 구현이 이미 덮었는지).
  - Task 7 Step 2의 변이 세 개가 각각 빨간불을 만들었는지. **하나라도 실패하지 않았다면 그 테스트 이름과 함께 "물리는 것을 보이지 못했다"고 적는다.**
  - 클립에서 실제로 본 것(특히 F-13의 미끄러짐 여부)과 클릭 로그의 적중/비적중 개수.
  - `fleeSpeedScale`을 클립을 보고 조정했다면 최종값과 이유.
  - HUD 버튼 가드의 수동 확인 결과.

- [ ] **Step 4: 자체 검토** — 커밋 전에 세 가지를 본다.
  1. **사양 대응**: F-09·F-10·F-11·F-12·F-13 각각에 대응하는 태스크가 있는가. 설계 문서 결정 표의 모든 행에 대응하는 태스크가 있는가.
  2. **자리표시자 검사**:
     ```bash
     cd /Users/hans/dev/aquarium && grep -rn "TBD\|TODO\|FIXME\|적절히\|비슷하게\|similar to Task" docs/superpowers/plans/2026-09-21-m5-click-flee.md rules/ unreal/Aquarium/Source/ scripts/render_m5_click.sh
     ```
     결과가 비어 있어야 한다.
  3. **이름·시그니처 일관성**: `PickFrontmostHit`(규칙과 서브시스템 두 곳), `ClickTarget`, `ApplyFleeFrom`, `FleeState`, `AsClickTarget`, `HandleClickRay`, `HandleClickAt`, `ParseAutoClick`, `BuildAutoClicks`, `FAutoClick`, `SpeedScale`, `fleeSpeedScale`이 헤더·구현·테스트·스크립트·계획에서 전부 같은 철자와 인자 목록인가.

- [ ] **Step 5: 최종 검증 후 커밋**

```bash
cd /Users/hans/dev/aquarium
ctest --test-dir build 2>&1 | tail -2
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
git status --short
```
기대: `100% tests passed out of 103` (**`0 tests failed out of`가 아니다**), `60`.

```bash
cd /Users/hans/dev/aquarium && git add docs/TASK.md docs/SETUP.md docs/superpowers/plans/2026-09-21-m5-click-flee.md && git commit -m "$(cat <<'EOF'
docs: M5 결과 기록 (클릭 도망, 회복, 애니메이션)

규칙 90 -> 103, Automation 47 -> 60. TASK.md의 "미구현은 F-09
레이캐스트뿐"이라는 기록을 실제 상태로 바로잡았다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

- [ ] **Step 6: 사용자 시각 검토 요청** — M4a·M4b·M4c와 같은 형식으로, 클립 `-m5-click.mp4`와 비교 스틸 `-m5-compare.png`, 그리고 **클릭 로그 CSV**를 제시하고 **"클립이 판정 대상이고 스틸은 회귀 확인용"** 임을 명시한다. 알려진 한계(역투영 경로는 Automation으로 검증되지 않음, HUD 버튼 가드는 수동 확인, 도망 중에는 분리도 함께 꺼짐)를 숨기지 않고 함께 적는다.

---

## 구현 중 발견한 후속 항목

*(실행 중 채운다. 비워 둔 채로 M5를 닫지 않는다.)*

- **M4c에서 이월(그대로 열려 있음)** — 동종 무리를 눈으로 완전히 확정하지 못함, `SpeedSurvivesPropAvoidance`의 15% 여유, 프레임 정렬되지 않은 소품 회피 전/후 비교, 5.4%로 얇은 성능 여유. 상세는 [M4c 구현 계획](2026-09-21-m4c-schooling.md)의 같은 절.
- **M4b에서 이월** — TubeCoral 실루엣, FanCoral 구분, PlateCoral·BrainCoral의 흰 덩어리, 스카이라이트 상향에 따른 바닥 밝기, `GOBO_COARSE_THRESHOLD` 절충.
- **M4a에서 이월** — 비늘 이방성 셀, 가슴지느러미 위치·크기, 나비고기 `pecStray=16`, 고정 바운드의 `BoundsScale`, 미사용 `SK_*_PhysicsAsset`.
- **M3에서 이월** — F-06(포커스 상실 일시정지)의 실제 윈도 포커스 델리게이트는 헤드리스에서 발생하지 않는다. 창 모드 수동 확인 1회가 여전히 필요하다.
- **M5가 새로 여는 항목(설계 단계에서 이미 아는 것)**
  - **역투영 경로는 Automation으로 검증되지 않는다** — `-nullrhi`에 게임 뷰포트가 없다. 1차 방어선은 실제 실행의 클릭 로그 CSV다. `PropMaterialsCompile`이 셰이더 컴파일을 검사하지 못하는 것과 같은 종류의 한계.
  - **HUD 나가기 버튼 가드는 수동 확인에만 의존한다** — 헤드리스에서 호버가 발생하지 않는다.
  - **도망 중에는 분리(보이즈)도 함께 꺼진다** — 무리를 통째로 건너뛰기 때문이다. 0.8초 동안 다른 종과 겹칠 수 있다.
