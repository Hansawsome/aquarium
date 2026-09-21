# M4c — 무리(보이즈) 행동·소품 충돌·수직 전환 롤 제거 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 배경 물고기가 같은 종끼리 느슨한 무리를 이루게 하고(규칙 계층 보이즈), 산호·바위를 접촉 전에 미리 돌아가게 하며(규칙 계층 장애물 조향), M2부터 남아 있던 수직 전환 시 0.33초 피루엣을 없앤다(swing/twist 분해). 규칙 66 → **90**, Automation 38 → **47**. 플레이어 물고기는 무리 규칙을 전혀 받지 않는다.

**Architecture:** 세 행동 전부 `rules/include/aquarium/`의 순수 함수(`Boids.h`, `Obstacles.h`, `Facing.h`)로 들어간다. Unreal 쪽은 `UFishSchoolSubsystem`이 이웃 스냅샷과 장애물 원판을 **실제 액터에서 파생해** 공급하고, `AFishActor`가 그 결과를 소비한다. 소품 반경 표를 C++로 베끼지 않는다. 판정은 **클립**(스틸이 아니라)과 항목별 성능 토글.

**Tech Stack:** C++17 규칙 계층 + Catch2, Unreal 5.8.2 (C++20 모듈 + 에디터 Python), ffmpeg.

**공통 명령**:
- 규칙 테스트: `cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j && ctest --test-dir build`
- UE 빌드(**반드시 두 번**): `"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex`
- UE 파이썬: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/<s>.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "_OK|Traceback|LogPython: Error|Failed to compile Material"`
- Automation: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "Test Completed|Tests Failed"`

**모든 태스크에 걸리는 규약 여섯 가지 (전부 이 프로젝트가 실제로 대가를 치른 것):**
1. **에디터 Python은 `-FullStdOutLogOutput` 없이 돌리면 아무것도 출력하지 않는다** — `REEF_OK`도, `AssertionError`도. 그런데 레벨은 덮어쓴다. **종료 코드는 무관한 `GameFeatureData` 오류 때문에 항상 1이다. 판정은 `*_OK` grep으로만 한다.**
2. **UE 모듈은 자동화 전에 두 번 빌드한다.** UBT가 `UnrealEditor.modules`를 한 빌드 늦게 쓴다.
3. **머티리얼은 조용히 실패하고 회색 기본 머티리얼로 떨어지면서 에디터 뷰포트에서는 멀쩡해 보인다.** M4c는 머티리얼을 건드리지 않지만 캡처 실행 로그에서 `Failed to compile Material` 0건을 반드시 grep한다(`-nullrhi`가 **아닌** 실제 게임 실행 로그여야 한다).
4. **테스트는 아무것도 검사하지 않으면서 통과할 수 있다** (`PropMaterialsCompile`이 38/38 Success를 내면서 `checked 0 of 13`을 찍었다). **이 계획의 모든 신규 테스트는 빨간불이 켜지는 것을 먼저 확인한다.**
5. **가드는 너무 느슨하면 진짜 결함을 통과시킨다** (퇴화 노멀맵이 분산 2.37e-06으로 통과했다). 한도를 정할 때는 "실제 결함 값이 이 한도에 걸리는가"를 숫자로 확인한다.
6. **같은 규칙을 두 군데 두면 검증이 결함을 영원히 못 잡는다** (프롭 레인 회피가 `build_reef_m1.py`와 `verify_scene.py` 양쪽에 있었다). 소품 반경은 **액터 바운드에서 파생**하고, 테스트 기대값은 **규칙 계층 함수에서 파생**한다.

---

### Task 0: 기준 보관과 현재 상태 확인

**Files:** 없음 (읽기 전용)

- [ ] **Step 1: 브랜치와 작업 트리 확인**

```bash
cd /Users/hans/dev/aquarium
git status --short && git rev-parse --abbrev-ref HEAD && git rev-parse --short HEAD
```
기대: 첫 줄 출력 없음, `feat/m4c-schooling`, `631b920`.

- [ ] **Step 2: 현재 규칙 테스트 66개 통과 확인**

```bash
cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j 2>&1 | tail -3 && ctest --test-dir build 2>&1 | tail -3
```
기대: `100% tests passed, 0 tests failed out of 66`.

- [ ] **Step 3: 기준 산출물 보관**

```bash
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m4c_before
mkdir -p "$B"
cd /Users/hans/dev/aquarium
cp docs/reviews/2026-09-21-m4b-scene.png "$B/before-scene.png"
cp docs/reviews/2026-09-21-m4b-reef.mp4 "$B/before-reef.mp4"
cp docs/reviews/2026-09-21-m4b-perf.md "$B/before-perf.md"
ls -l "$B"
```

- [ ] **Step 4: 개선 전 클립을 직접 본다** — `$B/before-reef.mp4`. 확인할 것 세 가지를 눈으로 기록한다: (1) 같은 종이 몰려다니는 장면이 있는가(없어야 한다), (2) 물고기가 산호를 통과하는 순간이 있는가, (3) 수직 전환 시 제자리 회전이 보이는가. **이 기록이 뒤에서 "좋아졌다"를 주장할 때의 근거다.**

---

### Task 1: 규칙 계층 — 보이즈 (`Boids.h`)

**Files:** Create `rules/include/aquarium/Boids.h`, Create `tests/test_boids.cpp`, Modify `CMakeLists.txt`

- [ ] **Step 1 (RED): 테스트 파일부터 만든다**

`tests/test_boids.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "aquarium/Boids.h"

using aquarium::BoidNeighbor;
using aquarium::BoidsParams;
using aquarium::BoidsResult;
using aquarium::SchoolingSteer;
using aquarium::BlendSteering;
using aquarium::Vec2;

namespace {
BoidNeighbor Mate(float x, float y, float vx, float vy, int species = 1) {
    BoidNeighbor n;
    n.position = {x, y};
    n.velocity = {vx, vy};
    n.depth = 0.f;
    n.species = species;
    n.avoidOnly = false;
    return n;
}
} // namespace

TEST_CASE("no neighbours produces no steer", "[boids]") {
    const BoidsParams p;
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, nullptr, 0, p);
    CHECK(r.steer.Length() == 0.f);
    CHECK(r.consideredCount == 0);
    CHECK(r.avoidCount == 0);
}

TEST_CASE("cohesion pulls toward a distant school mate", "[boids]") {
    const BoidsParams p;
    const BoidNeighbor n[] = {Mate(100.f, 0.f, 1.f, 0.f)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, n, 1, p);
    CHECK(r.consideredCount == 1);
    CHECK(r.avoidCount == 0);
    CHECK(r.steer.x > 0.9f);               // toward it
}

TEST_CASE("separation pushes away from a too-close mate", "[boids]") {
    const BoidsParams p;
    const BoidNeighbor n[] = {Mate(10.f, 0.f, 1.f, 0.f)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, n, 1, p);
    CHECK(r.avoidCount == 1);
    CHECK(r.steer.x < 0.f);                // separation outweighs cohesion at 10 cm
}

TEST_CASE("alignment matches the school heading", "[boids]") {
    BoidsParams p;
    p.cohesionWeight = 0.f;                // isolate alignment
    p.separationWeight = 0.f;
    // Two mates at equal and opposite offsets: cohesion cancels, only alignment survives.
    const BoidNeighbor n[] = {Mate(80.f, 0.f, 0.f, 1.f), Mate(-80.f, 0.f, 0.f, 1.f)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, n, 2, p);
    CHECK(r.consideredCount == 2);
    CHECK(r.steer.y > 0.99f);
}

TEST_CASE("other species never align or cohere", "[boids]") {
    const BoidsParams p;
    const BoidNeighbor n[] = {Mate(100.f, 0.f, 1.f, 0.f, /*species*/ 2)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, /*species*/ 1, n, 1, p);
    CHECK(r.consideredCount == 0);
    CHECK(r.steer.Length() == 0.f);        // 100 cm away, so no separation either
}

TEST_CASE("other species are still separated from", "[boids]") {
    const BoidsParams p;
    const BoidNeighbor n[] = {Mate(20.f, 0.f, 1.f, 0.f, /*species*/ 2)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, /*species*/ 1, n, 1, p);
    CHECK(r.consideredCount == 0);
    CHECK(r.avoidCount == 1);
    CHECK(r.steer.x < -0.9f);
}

TEST_CASE("the player fish is avoided but never followed", "[boids]") {
    const BoidsParams p;
    BoidNeighbor player = Mate(90.f, 0.f, 1.f, 0.f, /*species*/ 1); // SAME species on purpose
    player.avoidOnly = true;
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, &player, 1, p);
    CHECK(r.consideredCount == 0);         // never a cohesion/alignment target
    CHECK(r.avoidCount == 1);              // 90 cm is inside avoidOnlyRadius 110
    CHECK(r.steer.x < -0.9f);              // pushed away, so the school opens around the player
}

TEST_CASE("neighbours on a distant plane are ignored", "[boids]") {
    const BoidsParams p;
    BoidNeighbor far_ = Mate(30.f, 0.f, 1.f, 0.f);
    far_.depth = 400.f;                    // 4 m behind, well past depthRadius 120
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, &far_, 1, p);
    CHECK(r.consideredCount == 0);
    CHECK(r.avoidCount == 0);
    CHECK(r.steer.Length() == 0.f);
}

TEST_CASE("maxNeighbors caps how many mates contribute", "[boids]") {
    BoidsParams p;
    p.maxNeighbors = 2;
    const BoidNeighbor n[] = {Mate(100.f, 0.f, 1.f, 0.f), Mate(100.f, 10.f, 1.f, 0.f),
                              Mate(100.f, 20.f, 1.f, 0.f), Mate(100.f, 30.f, 1.f, 0.f)};
    const BoidsResult r = SchoolingSteer({0.f, 0.f}, 0.f, 1, n, 4, p);
    CHECK(r.consideredCount == 2);
}

TEST_CASE("BlendSteering keeps a unit vector and never returns zero for opposed inputs", "[boids]") {
    const Vec2 own{1.f, 0.f};
    CHECK(BlendSteering(own, {0.f, 0.f}, 0.f).x == 1.f);
    const Vec2 mixed = BlendSteering(own, {0.f, 1.f}, 0.5f);
    CHECK_THAT(mixed.Length(), Catch::Matchers::WithinAbs(1.0, 1e-4));
    CHECK(mixed.x > 0.f);
    CHECK(mixed.y > 0.f);
    // Exactly opposed: the sum is the zero vector, and returning it would let StepMotion
    // decelerate the fish through zero -- the M1/M3 heading-flip bug. Keep the fish's own intent.
    const Vec2 opposed = BlendSteering(own, {-1.f, 0.f}, 0.5f);
    CHECK(opposed.x == 1.f);
}
```

`CMakeLists.txt`의 `add_executable(rules_tests` 목록에 `tests/test_boids.cpp`를 추가한다(`tests/test_heading.cpp` 다음 줄).

```bash
cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j 2>&1 | grep -E "Boids.h|error" | head -5
```
기대(RED): `fatal error: 'aquarium/Boids.h' file not found`.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Boids.h` 구현**

```cpp
#pragma once
#include <cstddef>

#include "aquarium/Vec2.h"

namespace aquarium {

// One other fish as seen by the fish being steered.
//
// `position`/`velocity` are in the SHARED swim frame (x = world Y, y = world Z), not in either
// fish's local plane coordinates. Every swim plane in this game uses right = +Y and up = +Z, so
// adding the plane origin back gives one common 2D frame, and a direction expressed in it is
// usable by any fish without conversion.
//
// `depth` is the world X of the neighbour's plane. Background fish live on 36 parallel planes
// between X = 330 and X = 700, so two fish that overlap on screen can be 3 m apart. Schooling
// them together would look correct head-on and absurd from any other angle.
struct BoidNeighbor {
    Vec2 position;
    Vec2 velocity;
    float depth = 0.f;
    int species = 0;
    // The player's fish. Never followed (no alignment, no cohesion), only avoided, and with a
    // bigger radius, so the school opens up around exactly the fish the child is watching
    // instead of crowding it.
    bool avoidOnly = false;
};

struct BoidsParams {
    float neighborRadius = 140.f;   // cm in the shared swim frame
    float depthRadius = 120.f;      // cm along world X
    float separationRadius = 45.f;
    float avoidOnlyRadius = 110.f;  // separation radius used for avoidOnly neighbours
    float separationWeight = 1.7f;
    float alignmentWeight = 0.6f;
    float cohesionWeight = 0.35f;
    int maxNeighbors = 6;           // hard cap so one dense clump cannot dominate the steer
};

struct BoidsResult {
    Vec2 steer;                  // unit vector, or zero when nothing applied
    int consideredCount = 0;     // school mates that contributed alignment/cohesion
    int avoidCount = 0;          // neighbours of any species that contributed separation
};

inline bool IsSchoolMate(int selfSpecies, const BoidNeighbor& n) {
    return !n.avoidOnly && n.species == selfSpecies;
}

// Separation + alignment + cohesion over `neighbors`, returned as a unit direction.
//
// Cost note: this is deliberately an all-pairs scan with no spatial structure. There are 36
// background fish, so a full frame is 36 x 35 = 1260 iterations of a handful of arithmetic ops.
// A uniform grid would cost more to maintain than it saves at this n. If a measurement ever
// shows this above the ~1.3 fps run-to-run noise floor, THEN add one.
inline BoidsResult SchoolingSteer(Vec2 pos, float depth, int species,
                                  const BoidNeighbor* neighbors, std::size_t count,
                                  const BoidsParams& p)
{
    BoidsResult r;
    if (neighbors == nullptr || count == 0) return r;

    Vec2 sep{0.f, 0.f};
    Vec2 ali{0.f, 0.f};
    Vec2 coh{0.f, 0.f};
    const int cap = p.maxNeighbors > 0 ? p.maxNeighbors : 0;

    for (std::size_t i = 0; i < count; ++i) {
        const BoidNeighbor& n = neighbors[i];
        const Vec2 off = n.position - pos;
        const float dist = off.Length();
        if (dist <= 1e-4f) continue;   // self, or exactly coincident: no direction to use
        const float depthGap = n.depth - depth;
        const float depthAbs = depthGap < 0.f ? -depthGap : depthGap;
        if (depthAbs > p.depthRadius) continue;

        const float sepRadius = n.avoidOnly ? p.avoidOnlyRadius : p.separationRadius;
        if (dist < sepRadius && sepRadius > 0.f) {
            // Inverse falloff: the closer it is, the harder the push away from it.
            sep = sep + off.Normalized() * (-(sepRadius - dist) / sepRadius);
            ++r.avoidCount;
        }
        if (r.consideredCount >= cap) continue;
        if (!IsSchoolMate(species, n)) continue;   // follow your own species only
        if (dist > p.neighborRadius) continue;
        ali = ali + n.velocity.Normalized();
        coh = coh + off;
        ++r.consideredCount;
    }

    if (r.consideredCount > 0) {
        const float inv = 1.f / static_cast<float>(r.consideredCount);
        ali = ali * inv;
        coh = coh * inv;
    }
    const Vec2 sum = sep * p.separationWeight + ali.Normalized() * p.alignmentWeight +
                     coh.Normalized() * p.cohesionWeight;
    r.steer = sum.Normalized();
    return r;
}

// Blends a fish's own desired direction with its schooling steer. weight 0 -> own direction only,
// weight 1 -> school only. The result is always a unit vector, so the caller hands it straight to
// the boundary rule and StepMotion without changing the fish's speed.
inline Vec2 BlendSteering(Vec2 ownDir, Vec2 schoolDir, float weight) {
    if (weight <= 0.f || schoolDir.Length() <= 1e-6f) return ownDir.Normalized();
    if (weight >= 1.f) return schoolDir.Normalized();
    const Vec2 mix = ownDir.Normalized() * (1.f - weight) + schoolDir.Normalized() * weight;
    const Vec2 unit = mix.Normalized();
    // Exactly opposed inputs sum to zero. Returning zero would make StepMotion decelerate the
    // fish through zero speed, and a velocity that passes through zero has no stable heading --
    // the 180-degree facing flip fixed in M1/M3. Keep the fish's own intent instead.
    if (unit.Length() <= 1e-6f) return ownDir.Normalized();
    return unit;
}

} // namespace aquarium
```

```bash
cd /Users/hans/dev/aquarium && cmake --build build -j 2>&1 | grep -E "warning|error" ; ctest --test-dir build 2>&1 | tail -3
```
기대: 경고 0, `100% tests passed, 0 tests failed out of 76`.

- [ ] **Step 3: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules/include/aquarium/Boids.h tests/test_boids.cpp CMakeLists.txt && git commit -m "$(cat <<'EOF'
feat: 규칙 계층 보이즈(분리·정렬·응집) 추가

같은 종만 정렬·응집하고 분리는 전 종에 적용한다. 플레이어 물고기는
avoidOnly로 넘어와 회피 대상만 되고 따라가는 대상은 절대 되지 않는다.
공유 좌표계 (worldY, worldZ) + 평면 깊이 게이트로 36장 평면을 구분한다.
규칙 테스트 66 -> 76.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 2: 규칙 계층 — 소품 회피 (`Obstacles.h`)

**Files:** Create `rules/include/aquarium/Obstacles.h`, Create `tests/test_obstacles.cpp`, Modify `CMakeLists.txt`

- [ ] **Step 1 (RED): 테스트부터**

`tests/test_obstacles.cpp`:

```cpp
#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "aquarium/Obstacles.h"

using aquarium::Obstacle;
using aquarium::ObstacleParams;
using aquarium::SteerAroundObstacles;
using aquarium::Vec2;

TEST_CASE("no obstacles leaves the direction untouched", "[obstacles]") {
    const ObstacleParams p;
    const Vec2 dir{3.f, 4.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, nullptr, 0, p);
    CHECK(out.x == dir.x);
    CHECK(out.y == dir.y);
}

TEST_CASE("an obstacle dead ahead turns the fish without changing its speed", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{80.f, 0.f}, 30.f}};
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    // This is the whole point of the rule: M3 proved that zeroing the blocked component lets the
    // velocity decelerate through zero, and a zero velocity has no heading, which flipped the
    // visible facing 180 degrees. Magnitude must survive every path through this function.
    CHECK_THAT(out.Length(), Catch::Matchers::WithinAbs(dir.Length(), 1e-3));
    CHECK(out.y != 0.f);                 // actually turned
    CHECK(out.x > 0.f);                  // still going forward, not reversed
}

TEST_CASE("a dead-on obstacle picks the same side every time", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{80.f, 0.f}, 30.f}};
    const Vec2 a = SteerAroundObstacles({0.f, 0.f}, {50.f, 0.f}, o, 1, p);
    const Vec2 b = SteerAroundObstacles({0.f, 0.f}, {50.f, 0.f}, o, 1, p);
    CHECK(a.x == b.x);
    CHECK(a.y == b.y);
    CHECK(a.y > 0.f);                    // documented choice: left of the heading
}

TEST_CASE("an obstacle behind the fish is ignored", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{-80.f, 0.f}, 30.f}};
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    CHECK(out.x == dir.x);
    CHECK(out.y == dir.y);
}

TEST_CASE("an obstacle the ray misses laterally is ignored", "[obstacles]") {
    const ObstacleParams p;                         // margin 20
    const Obstacle o[] = {{{80.f, 100.f}, 30.f}};   // 100 cm off to the side, reach is 50
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    CHECK(out.x == dir.x);
    CHECK(out.y == dir.y);
}

TEST_CASE("an obstacle beyond lookAhead is ignored", "[obstacles]") {
    ObstacleParams p;
    p.lookAhead = 100.f;
    const Obstacle o[] = {{{300.f, 0.f}, 30.f}};
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    CHECK(out.x == dir.x);
    CHECK(out.y == dir.y);
}

TEST_CASE("a fish already inside an obstacle is steered out at the same speed", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{5.f, 0.f}, 40.f}};   // the fish is inside the padded disc
    const Vec2 dir{50.f, 0.f};
    const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 1, p);
    CHECK_THAT(out.Length(), Catch::Matchers::WithinAbs(dir.Length(), 1e-3));
    CHECK(out.y != 0.f);                 // pushed sideways rather than teleported
}

TEST_CASE("magnitude is preserved on every path, including many obstacles", "[obstacles]") {
    const ObstacleParams p;
    const Obstacle o[] = {{{60.f, 5.f}, 25.f}, {{100.f, -10.f}, 40.f}, {{-50.f, 0.f}, 90.f},
                          {{40.f, 200.f}, 10.f}, {{0.f, 0.f}, 0.f}};
    for (int i = 0; i < 36; ++i) {
        const float a = static_cast<float>(i) * 10.f * 3.14159265f / 180.f;
        const Vec2 dir{37.5f * std::cos(a), 37.5f * std::sin(a)};
        const Vec2 out = SteerAroundObstacles({0.f, 0.f}, dir, o, 5, p);
        CHECK_THAT(out.Length(), Catch::Matchers::WithinAbs(37.5, 1e-3));
    }
}
```

`CMakeLists.txt`에 `tests/test_obstacles.cpp`를 추가한다.

```bash
cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j 2>&1 | grep -E "Obstacles.h|error" | head -5
```
기대(RED): `fatal error: 'aquarium/Obstacles.h' file not found`.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Obstacles.h` 구현**

```cpp
#pragma once
#include <cstddef>

#include "aquarium/Vec2.h"

namespace aquarium {

// A prop as it intersects one swim plane: a disc in that plane's local 2D coordinates.
// The engine side derives centre and radius from the actual placed actor's bounds; nothing in
// this layer knows what a coral or a rock is, or how many of them there are.
struct Obstacle {
    Vec2 center;
    float radius = 0.f;
};

struct ObstacleParams {
    float lookAhead = 150.f;   // cm of travel ahead of the fish that is considered
    float margin = 20.f;       // cm of clearance added to every radius
};

// Steers `dir` around the nearest threatening obstacle while preserving its magnitude.
//
// This has the same shape as SteerAlongBoundary, and for the same hard-won reason: killing the
// component of the desired direction that points at an obstacle drops the desired speed to zero,
// so the velocity decelerates through zero and re-accelerates the other way. A velocity that
// passes through zero has no stable heading, which flipped the visible facing by 180 degrees in
// one step (M1 review, fixed in M3). Every return path here is either `dir` unchanged or a
// rotated vector of the SAME length, so a fish never stalls in front of a coral.
//
// It is also deliberately not a physics response: a physics response acts AFTER contact, which
// is by definition a bounce. Steering before contact is what makes the fish look like it saw the
// coral coming.
inline Vec2 SteerAroundObstacles(Vec2 pos, Vec2 dir, const Obstacle* obstacles, std::size_t count,
                                 const ObstacleParams& p)
{
    const float len = dir.Length();
    if (len <= 1e-6f || obstacles == nullptr || count == 0) return dir;
    const Vec2 fwd = dir * (1.f / len);
    const Vec2 side{-fwd.y, fwd.x};   // left of the heading

    float bestAhead = p.lookAhead;
    const Obstacle* best = nullptr;
    float bestLateral = 0.f;
    for (std::size_t i = 0; i < count; ++i) {
        const Obstacle& o = obstacles[i];
        const float reach = o.radius + p.margin;
        if (reach <= 0.f) continue;
        const Vec2 off = o.center - pos;
        const float ahead = off.x * fwd.x + off.y * fwd.y;
        if (ahead < -reach || ahead > p.lookAhead) continue;   // behind, or too far to matter yet
        const float lateral = off.x * side.x + off.y * side.y;
        const float lateralAbs = lateral < 0.f ? -lateral : lateral;
        if (lateralAbs >= reach) continue;                     // the heading ray misses it
        if (ahead >= bestAhead) continue;                      // a nearer threat already won
        bestAhead = ahead;
        best = &o;
        bestLateral = lateral;
    }
    if (best == nullptr) return dir;

    // Push sideways, away from the obstacle centre, by as much as the overlap demands: nothing at
    // the rim, a full 45 degrees dead-on. A fish that is exactly dead-on (lateral == 0) has no
    // preferred side, so it always takes the left one. An arbitrary but STABLE choice beats a coin
    // flip that could flutter between frames and shake the fish.
    const float reach = best->radius + p.margin;
    const float lateralAbs = bestLateral < 0.f ? -bestLateral : bestLateral;
    const float push = (reach - lateralAbs) / reach;           // 0 at the rim, 1 dead-on
    const float sign = bestLateral > 0.f ? -1.f : 1.f;         // away from the centre
    const Vec2 turned = fwd + side * (sign * push);
    const Vec2 unit = turned.Normalized();
    if (unit.Length() <= 1e-6f) return dir;                    // unreachable (|turned| >= 1), kept
    return unit * len;                                         // magnitude preserved, always
}

} // namespace aquarium
```

```bash
cd /Users/hans/dev/aquarium && cmake --build build -j 2>&1 | grep -E "warning|error" ; ctest --test-dir build 2>&1 | tail -3
```
기대: 경고 0, `100% tests passed, 0 tests failed out of 84`.

- [ ] **Step 3: 가드가 실제로 무는지 확인(규약 5)** — `Obstacles.h`의 `return unit * len;`을 잠시 `return unit * (len * 0.5f);`로 바꾸고 다시 돌린다.

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/return unit \* len;/return unit * (len * 0.5f);/' rules/include/aquarium/Obstacles.h && cmake --build build -j >/dev/null 2>&1 && ctest --test-dir build 2>&1 | tail -3
```
기대: 실패한 테스트가 최소 3개(`an obstacle dead ahead...`, `a fish already inside...`, `magnitude is preserved...`). 확인 뒤 되돌린다:

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/return unit \* (len \* 0.5f);/return unit * len;/' rules/include/aquarium/Obstacles.h && cmake --build build -j >/dev/null 2>&1 && ctest --test-dir build 2>&1 | tail -3
```
기대: 다시 `100% tests passed ... out of 84`.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules/include/aquarium/Obstacles.h tests/test_obstacles.cpp CMakeLists.txt && git commit -m "$(cat <<'EOF'
feat: 규칙 계층 소품 회피 조향 추가

SteerAlongBoundary와 같은 모양으로, 접촉 전에 옆으로 미끄러지되 입력
크기를 모든 경로에서 보존한다. M3에서 속도가 0을 통과해 방향이 180도
뒤집힌 버그를 소품에서 다시 만들지 않기 위한 계약이고, 전용 테스트가
지킨다. 규칙 테스트 76 -> 84.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 3: 규칙 계층 — swing/twist 제한 (`Facing.h`)

**Files:** Create `rules/include/aquarium/Facing.h`, Create `tests/test_facing.cpp`, Modify `CMakeLists.txt`

- [ ] **Step 1 (RED): 테스트부터**

`tests/test_facing.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "aquarium/Facing.h"

using aquarium::FacingParams;
using aquarium::Steepness;
using aquarium::MaxSwingStepDeg;
using aquarium::MaxTwistStepDeg;

TEST_CASE("a non-positive step permits no rotation at all", "[facing]") {
    const FacingParams p;
    CHECK(MaxSwingStepDeg(p, 0.f) == 0.f);
    CHECK(MaxSwingStepDeg(p, -0.1f) == 0.f);
    CHECK(MaxTwistStepDeg(1.f, p, 0.f) == 0.f);
    CHECK(MaxTwistStepDeg(1.f, p, -0.1f) == 0.f);
}

TEST_CASE("a shallow heading twists at the ordinary turn rate", "[facing]") {
    const FacingParams p;
    CHECK(Steepness(0.f, p) == 0.f);
    CHECK(Steepness(0.5f, p) == 0.f);
    CHECK_THAT(MaxTwistStepDeg(0.f, p, 0.1f), Catch::Matchers::WithinAbs(54.0, 1e-3));
}

TEST_CASE("a vertical heading twists at the fast rate", "[facing]") {
    const FacingParams p;
    CHECK_THAT(Steepness(1.f, p), Catch::Matchers::WithinAbs(1.0, 1e-5));
    // 2880 deg/s spends the unavoidable 180 degree flip in 62 ms, about 4 frames at 60 fps,
    // while the fish is end-on to the camera and the flip cannot be read as a pirouette.
    CHECK_THAT(MaxTwistStepDeg(1.f, p, 0.1f), Catch::Matchers::WithinAbs(288.0, 1e-3));
    CHECK(180.f / p.steepRollRateDegPerSec < 0.07f);
}

TEST_CASE("steepness is monotonic and clamped to 0..1", "[facing]") {
    const FacingParams p;
    float prev = -1.f;
    for (int i = 0; i <= 20; ++i) {
        const float s = static_cast<float>(i) / 20.f;
        const float v = Steepness(s, p);
        CHECK(v >= 0.f);
        CHECK(v <= 1.f);
        CHECK(v >= prev);
        prev = v;
    }
    CHECK(Steepness(1.5f, p) == Steepness(1.f, p));   // clamped past vertical
}

TEST_CASE("a dive and a climb are equally steep", "[facing]") {
    const FacingParams p;
    for (float s : {0.0f, 0.3f, 0.7f, 0.85f, 1.0f}) {
        CHECK(Steepness(s, p) == Steepness(-s, p));
        CHECK(MaxTwistStepDeg(s, p, 0.05f) == MaxTwistStepDeg(-s, p, 0.05f));
    }
}

TEST_CASE("the swing limit is independent of steepness", "[facing]") {
    const FacingParams p;
    CHECK_THAT(MaxSwingStepDeg(p, 0.05f), Catch::Matchers::WithinAbs(27.0, 1e-3));
    // Degenerate params must not divide by zero or invert the rates.
    FacingParams d;
    d.steepBeginSin = 1.f;
    CHECK(Steepness(1.f, d) >= 0.f);
    d.steepRollRateDegPerSec = 10.f;   // slower than upright: clamped up, never down
    CHECK(MaxTwistStepDeg(1.f, d, 0.1f) >= MaxTwistStepDeg(0.f, d, 0.1f));
}
```

`CMakeLists.txt`에 `tests/test_facing.cpp`를 추가한다.

```bash
cd /Users/hans/dev/aquarium && cmake -S . -B build && cmake --build build -j 2>&1 | grep -E "Facing.h|error" | head -5
```
기대(RED): `fatal error: 'aquarium/Facing.h' file not found`.

- [ ] **Step 2 (GREEN): `rules/include/aquarium/Facing.h` 구현**

```cpp
#pragma once
#include <algorithm>
#include <cmath>

namespace aquarium {

// Separate limits for the two independent parts of a facing change.
//
// "Swing" turns the nose toward the new heading. "Twist" rolls the body about the nose-tail axis.
// They need different limits because of a fact about this game's geometry, diagnosed in the M4c
// design document:
//
//   Every fish swims on a vertical plane, and its facing frame is built from the heading plus
//   world up. Two headings that straddle the vertical -- say a hair short of straight up on one
//   side and a hair past it on the other -- produce frames that differ by a 180 degree TWIST
//   about the (almost vertical) forward axis. Slewing the whole quaternion at a single rate
//   spends that 180 degrees at the turn rate: 180 / 540 = 0.33 s. That is exactly the ~0.3 s
//   pirouette recorded in the M2 review and still present through M4b.
//
// The twist cannot be removed. A plane-bound fish that keeps its dorsal fin up has a frame that
// is necessarily discontinuous at the two vertical headings; that is topology, not a bug. So the
// goal is not to remove it but to SPEND IT WHERE IT CANNOT BE SEEN: while the heading is steep
// the fish is nearly end-on to this game's fixed horizontal camera, and a fast twist of a
// bilaterally symmetric body reads as a flicker rather than a roll.
struct FacingParams {
    float maxTurnRateDegPerSec = 540.f;      // swing: how fast the nose may sweep
    float uprightRollRateDegPerSec = 540.f;  // twist while the heading is shallow (M3 behaviour)
    float steepRollRateDegPerSec = 2880.f;   // twist while steep: 180 deg in 62 ms
    float steepBeginSin = 0.70f;             // |sin(pitch)| where the fast rate starts blending in
};

// 0 while the heading is shallower than steepBeginSin, 1 at exactly vertical, smooth between.
// `verticalSin` is the vertical component of the unit forward vector. Its sign is ignored: a dive
// and a climb are equally end-on to the camera.
inline float Steepness(float verticalSin, const FacingParams& p) {
    const float s = std::min(std::fabs(verticalSin), 1.f);
    const float begin = std::min(std::max(p.steepBeginSin, 0.f), 0.999f);
    if (s <= begin) return 0.f;
    const float t = std::min((s - begin) / (1.f - begin), 1.f);
    return t * t * (3.f - 2.f * t);   // smoothstep, so there is no rate step at the band edge
}

// Degrees the nose may swing this step.
inline float MaxSwingStepDeg(const FacingParams& p, float dt) {
    if (dt <= 0.f) return 0.f;
    return std::max(p.maxTurnRateDegPerSec, 0.f) * dt;
}

// Degrees the body may twist about the forward axis this step.
inline float MaxTwistStepDeg(float verticalSin, const FacingParams& p, float dt) {
    if (dt <= 0.f) return 0.f;
    const float slow = std::max(p.uprightRollRateDegPerSec, 0.f);
    // Clamped up, never down: a params struct that asks for a slower steep rate than the upright
    // rate would reintroduce the very defect this exists to remove.
    const float fast = std::max(p.steepRollRateDegPerSec, slow);
    return (slow + (fast - slow) * Steepness(verticalSin, p)) * dt;
}

} // namespace aquarium
```

```bash
cd /Users/hans/dev/aquarium && cmake --build build -j 2>&1 | grep -E "warning|error" ; ctest --test-dir build 2>&1 | tail -3
```
기대: 경고 0, `100% tests passed, 0 tests failed out of 90`.

- [ ] **Step 3: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add rules/include/aquarium/Facing.h tests/test_facing.cpp CMakeLists.txt && git commit -m "$(cat <<'EOF'
feat: 규칙 계층 swing/twist 제한 함수 추가

수직 전환 시의 180도 비틀림은 평면에 묶인 물고기의 위상적 사실이라
없앨 수 없다. 대신 heading이 가파를 때(카메라에 끝단으로 설 때)
2880도/초로 태워 62 ms에 끝내고, 완만할 때는 기존 540도/초를 지킨다.
규칙 테스트 84 -> 90.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 4: 수직 롤 진단 — 결함이 실제로 180도인지 먼저 측정한다

**Files:** Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

이 태스크는 **아무것도 고치지 않는다.** 설계 문서의 진단(수직을 사이에 둔 두 프레임이 전방축에 대한 180도 비틀림만큼 다르고, 540°/s로 0.33초가 걸린다)이 맞는지 숫자로 확인한다. **숫자가 설명과 다르면 거기서 멈추고 다시 진단한다.** M4b의 빛줄기에서 뻔한 설명(세기)이 틀렸던 것과 같은 이유다.

- [ ] **Step 1 (RED): 진단 테스트 추가** — `FishActorTests.cpp` 끝에 붙인다.

```cpp
// Diagnoses (and, after Task 5, guards) the vertical-transition pirouette recorded since M2.
//
// The fish is driven by hand through a heading sweep that crosses straight up: up-and-right,
// then straight up, then up-and-left. The facing frame is MakeFromXZ(Fwd, worldUp), whose local
// Z flips sign the moment the lateral component of an almost vertical heading changes sign, so
// the two frames differ by a 180 degree twist about the (almost vertical) forward axis.
//
// Twist is measured as the rotation about the forward axis, separated from the swing that aims
// the nose -- the same decomposition StepSwim uses. BEFORE Task 5 this test FAILS and prints the
// measured total twist and the number of steps it took; those numbers are the evidence for the
// diagnosis and belong in the plan's follow-up section.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorNoLongTwistAcrossVertical, "Aquarium.Fish.FacingHasNoLongTwistAcrossVertical",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorNoLongTwistAcrossVertical::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 11u);
	Fish->bPlayerControlled = true;   // drive the heading by hand, no wander in the way
	Fish->PlaneHalfWidth = 400.f;
	Fish->PlaneHalfHeight = 400.f;
	Fish->InitializeSwim();

	constexpr float Dt = 1.f / 60.f;
	// Hold each direction long enough for the velocity to actually reach it.
	const TArray<FVector2D> Legs = {FVector2D(0.35f, 1.f), FVector2D(0.f, 1.f), FVector2D(-0.35f, 1.f)};
	float TotalTwistDeg = 0.f;
	int TwistingSteps = 0;
	float PeakTwistRate = 0.f;

	Fish->SetInputDirection(Legs[0]);
	for (int i = 0; i < 90; ++i) Fish->StepSwim(Dt);   // settle onto the first heading

	FQuat Prev = Fish->GetActorQuat();
	for (int Leg = 1; Leg < Legs.Num(); ++Leg)
	{
		Fish->SetInputDirection(Legs[Leg]);
		for (int i = 0; i < 120; ++i)
		{
			Fish->StepSwim(Dt);
			const FQuat Now = Fish->GetActorQuat();
			// Swing takes Prev's forward to Now's forward; whatever is left is twist.
			const FQuat Swing = FQuat::FindBetweenNormals(Prev.GetAxisX(), Now.GetAxisX());
			FQuat Twist = Now * (Swing * Prev).Inverse();
			Twist.Normalize();
			if (Twist.W < 0.f) Twist = FQuat(-Twist.X, -Twist.Y, -Twist.Z, -Twist.W);
			FVector Axis; float AngleRad;
			Twist.ToAxisAndAngle(Axis, AngleRad);
			const float StepTwistDeg = FMath::RadiansToDegrees(AngleRad);
			if (StepTwistDeg > 0.5f)
			{
				TotalTwistDeg += StepTwistDeg;
				++TwistingSteps;
				PeakTwistRate = FMath::Max(PeakTwistRate, StepTwistDeg / Dt);
			}
			Prev = Now;
		}
	}

	// The flip itself is unavoidable, so this does NOT assert that no twist happens. It asserts
	// that the twist never drags on: at the steep rate 180 degrees fits in 0.07 s, which is 5
	// steps at 60 fps. The defect took 0.33 s, i.e. 20 steps. The gap between 5 and 20 is wide
	// enough that this guard actually bites (see the M4b lesson about guards that are too loose).
	const float ElapsedSec = static_cast<float>(TwistingSteps) * Dt;
	AddInfo(FString::Printf(TEXT("twist total %.1f deg over %d steps (%.3f s), peak %.0f deg/s"),
		TotalTwistDeg, TwistingSteps, ElapsedSec, PeakTwistRate));
	return TestTrue(FString::Printf(TEXT("twist across vertical took %.3f s (limit 0.12 s), total %.1f deg"), ElapsedSec, TotalTwistDeg),
		ElapsedSec < 0.12f);
}
```

- [ ] **Step 2: 모듈을 두 번 빌드하고 진단 테스트만 돌린다**

```bash
cd /Users/hans/dev/aquarium
for pass in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium.Fish.FacingHasNoLongTwistAcrossVertical; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "twist total|Test Completed"
```
기대(RED): `twist total` 줄의 합계가 **170~190도**, 소요 시간이 **0.30~0.36초**, peak가 **540 deg/s 부근**이고 테스트는 **Fail**.

- [ ] **Step 3: 숫자를 판정한다**
  - 합계가 180도 부근이고 시간이 0.33초 부근이면 **진단 확정** — Task 5로 간다. 측정값을 이 계획 끝의 "구현 중 발견한 후속 항목"에 그대로 적는다.
  - 합계가 180도와 크게 다르거나(예: 90도, 360도) 시간이 0.33초와 다르면 **진단이 틀린 것이다.** Task 5를 진행하지 말고 멈춘 뒤, 실제 목표 쿼터니언을 스텝별로 로그로 찍어 다시 진단한다. 틀린 설명 위에 고침을 얹지 않는다.
  - 비틀림이 아예 0으로 나오면 입력 레그가 수직을 지나가지 않은 것이다. `Legs`의 가로 성분(0.35)을 0.15로 줄여 다시 측정한다.

- [ ] **Step 4: 커밋 (빨간불 상태로 커밋한다 — 다음 태스크가 초록으로 만든다)**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp && git commit -m "$(cat <<'EOF'
test: 수직 전환 비틀림 진단 테스트 추가 (RED)

M2부터 기록된 0.3초 롤이 실제로 전방축에 대한 180도 비틀림인지
측정한다. 이 커밋 시점에는 일부러 실패한다. 측정값이 진단과 다르면
수정을 진행하지 않고 다시 진단한다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 5: 수직 롤 제거 — swing/twist 분해 적용

**Files:** Modify `unreal/Aquarium/Source/Aquarium/FishActor.h`, Modify `unreal/Aquarium/Source/Aquarium/FishActor.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 1: `FishActor.h` — 헤더 include와 프로퍼티·멤버 추가**

`#include "aquarium/Bounds.h"` 아래에 추가:

```cpp
#include "aquarium/Facing.h"
```

`MaxFacingTurnRate` 선언 바로 다음에 추가:

```cpp
	// Twist (roll about the nose-tail axis) is rate-limited SEPARATELY from the swing that aims
	// the nose. A plane-bound fish that keeps its dorsal fin up has a facing frame that is
	// discontinuous at the two vertical headings -- crossing straight up costs a 180 degree twist,
	// which at MaxFacingTurnRate took 0.33 s and read as the pirouette recorded since M2. The
	// twist cannot be removed (it is topology), so it is spent fast while the fish is steep and
	// therefore end-on to the fixed camera, where it cannot be read as a roll.
	// See aquarium::FacingParams and docs/superpowers/specs/2026-09-21-m4c-schooling-design.md.
	UPROPERTY(EditAnywhere, Category = "Swim", meta = (ClampMin = "1")) float SteepRollRate = 2880.f;
	UPROPERTY(EditAnywhere, Category = "Swim", meta = (ClampMin = "0", ClampMax = "0.999")) float SteepBeginSin = 0.70f;
```

private 멤버 영역의 `aquarium::SwimAnimParams AnimParams;` 다음 줄에 추가:

```cpp
	aquarium::FacingParams FacingParamsValue;
```

- [ ] **Step 2: `FishActor.cpp` — `InitializeSwim`에서 파라미터를 채운다**

`AnimParams.boneCount = SpineBoneNames().Num();` 바로 다음에 추가:

```cpp
	FacingParamsValue.maxTurnRateDegPerSec = MaxFacingTurnRate;
	// Shallow headings keep the M3 rate exactly, so nothing about ordinary swimming changes.
	FacingParamsValue.uprightRollRateDegPerSec = MaxFacingTurnRate;
	FacingParamsValue.steepRollRateDegPerSec = SteepRollRate;
	FacingParamsValue.steepBeginSin = SteepBeginSin;
```

- [ ] **Step 3: `FishActor.cpp` — 쿼터니언 각도 제한 헬퍼를 파일 상단(`#include` 다음, `AFishActor::AFishActor()` 앞)에 추가**

```cpp
namespace
{
// Shortest-arc form of Q, clamped to at most MaxAngleDeg.
FQuat LimitQuatAngle(const FQuat& InQ, float MaxAngleDeg)
{
	FQuat Q = InQ.GetNormalized();
	// A quaternion and its negation are the same rotation; the one with W >= 0 is the short way
	// round. Without this, ToAxisAndAngle can report an angle above 180 degrees and the clamp
	// below would spin the fish the long way.
	if (Q.W < 0.f)
	{
		Q = FQuat(-Q.X, -Q.Y, -Q.Z, -Q.W);
	}
	FVector Axis;
	float AngleRad = 0.f;
	Q.ToAxisAndAngle(Axis, AngleRad);
	const float MaxRad = FMath::DegreesToRadians(FMath::Max(MaxAngleDeg, 0.f));
	if (AngleRad <= MaxRad || Axis.IsNearlyZero())
	{
		return Q;
	}
	return FQuat(Axis.GetSafeNormal(), MaxRad);
}
} // namespace
```

- [ ] **Step 4: `FishActor.cpp` — 자세 슬루 블록을 교체**

`StepSwim` 안에서 아래 기존 블록을

```cpp
		const FQuat Current = GetActorQuat();
		const FQuat Next = bFirstHeading
			? Target
			: FMath::QInterpConstantTo(Current, Target, DeltaSeconds, FMath::DegreesToRadians(MaxFacingTurnRate));
		SetActorRotation(Next);

		if (!bFirstHeading)
		{
			const float AppliedTurnRate = FMath::RadiansToDegrees(Current.AngularDistance(Next)) / DeltaSeconds;
			TargetTurnRate = FMath::Sign(RawTurnRate) * FMath::Min(AppliedTurnRate, MaxFacingTurnRate);
		}
```

다음으로 바꾼다:

```cpp
		// Split the required rotation into a SWING (aims the nose at the new heading) and a TWIST
		// (rolls the body about the nose-tail axis) and rate-limit them separately.
		//
		// QInterpConstantTo treated both as one lump, which is what made the vertical crossing
		// cost 0.33 s: the frames on either side of straight up differ by a 180 degree twist, and
		// at 540 deg/s that is 0.333 s of visible pirouette. The swing limit is unchanged, so
		// ordinary turning looks exactly as it did in M3; only the twist is allowed to go fast,
		// and only while the heading is steep enough that the fish is end-on to the camera.
		const FQuat Current = GetActorQuat();
		FQuat Next = Target;
		float AppliedSwingDeg = 0.f;
		if (!bFirstHeading)
		{
			const FVector CurFwd = Current.GetAxisX();
			const FVector TgtFwd = Target.GetAxisX();
			// FindBetweenNormals on exactly opposed forwards picks an arbitrary perpendicular
			// axis. That is fine here: the result is still a 180 degree swing, and the clamp
			// below spreads it over many frames at MaxFacingTurnRate.
			const FQuat SwingFull = FQuat::FindBetweenNormals(CurFwd, TgtFwd);
			const FQuat TwistFull = Target * (SwingFull * Current).Inverse();
			const FQuat Swing = LimitQuatAngle(SwingFull, aquarium::MaxSwingStepDeg(FacingParamsValue, DeltaSeconds));
			// F.z is the vertical component of the unit forward vector, i.e. sin(pitch).
			const FQuat Twist = LimitQuatAngle(TwistFull, aquarium::MaxTwistStepDeg(F.z, FacingParamsValue, DeltaSeconds));
			Next = Twist * Swing * Current;
			Next.Normalize();
			AppliedSwingDeg = FMath::RadiansToDegrees(
				FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(CurFwd, Next.GetAxisX())), -1.f, 1.f)));
		}
		SetActorRotation(Next);

		if (!bFirstHeading)
		{
			// The body bend follows the SWING only. A twist is the body rotating about its own
			// long axis; there is no reason for that to curl the tail, and feeding the lumped
			// angular distance in would have spiked the bend during the vertical crossing.
			const float AppliedTurnRate = AppliedSwingDeg / DeltaSeconds;
			TargetTurnRate = FMath::Sign(RawTurnRate) * FMath::Min(AppliedTurnRate, MaxFacingTurnRate);
		}
```

- [ ] **Step 5: `FacingIsContinuous`를 재작성** — `FishActorTests.cpp`의 해당 테스트 본문(`constexpr float Dt = 0.05f;`부터 `return true;`까지)을 다음으로 교체한다.

```cpp
	// The old bound was a single lumped quaternion distance, which could not distinguish the nose
	// sweeping from the body twisting. The twist across a vertical heading now deliberately
	// exceeds that lump, so the bound is split into the two things it was really standing in for.
	// Both limits are DERIVED from the rules-layer functions rather than re-typed here: a bound
	// copied into the test is a bound that can silently disagree with the code it guards.
	constexpr float Dt = 0.05f;
	aquarium::FacingParams FP;
	FP.maxTurnRateDegPerSec = Fish->MaxFacingTurnRate;
	FP.uprightRollRateDegPerSec = Fish->MaxFacingTurnRate;
	FP.steepRollRateDegPerSec = Fish->SteepRollRate;
	FP.steepBeginSin = Fish->SteepBeginSin;
	const float MaxSwingDeg = aquarium::MaxSwingStepDeg(FP, Dt) + 1.f;

	Fish->StepSwim(Dt);
	FQuat PrevQuat = Fish->GetActorQuat();
	for (int i = 1; i < 300; ++i)
	{
		Fish->StepSwim(Dt);
		const FQuat Now = Fish->GetActorQuat();
		// (1) the nose may never sweep faster than MaxFacingTurnRate.
		const float SwingDeg = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(PrevQuat.GetAxisX(), Now.GetAxisX())), -1.f, 1.f)));
		if (!TestTrue(FString::Printf(TEXT("step %d: nose swung %.1f deg (limit %.1f)"), i, SwingDeg, MaxSwingDeg), SwingDeg < MaxSwingDeg))
		{
			return false;
		}
		// (2) the twist may never exceed what this step's steepness permits. The steepness is
		// taken from the heading the fish actually ended the step on.
		const float VerticalSin = static_cast<float>(Now.GetAxisX().Z);
		const float MaxTwistDeg = aquarium::MaxTwistStepDeg(VerticalSin, FP, Dt) + 1.f;
		const FQuat Swing = FQuat::FindBetweenNormals(PrevQuat.GetAxisX(), Now.GetAxisX());
		FQuat Twist = Now * (Swing * PrevQuat).Inverse();
		Twist.Normalize();
		if (Twist.W < 0.f) Twist = FQuat(-Twist.X, -Twist.Y, -Twist.Z, -Twist.W);
		FVector TwistAxis;
		float TwistRad = 0.f;
		Twist.ToAxisAndAngle(TwistAxis, TwistRad);
		const float TwistDeg = FMath::RadiansToDegrees(TwistRad);
		if (!TestTrue(FString::Printf(TEXT("step %d: body twisted %.1f deg (limit %.1f at sin %.3f)"), i, TwistDeg, MaxTwistDeg, VerticalSin), TwistDeg < MaxTwistDeg))
		{
			return false;
		}
		PrevQuat = Now;
	}
	return true;
```

`FishActorTests.cpp` 상단의 include 목록에 `#include "aquarium/Facing.h"`를 추가한다.

- [ ] **Step 6: 빌드 두 번 + 자세 관련 테스트 4개 실행**

```bash
cd /Users/hans/dev/aquarium
for pass in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium.Fish; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "twist total|Test Completed"
```
기대: `FacingHasNoLongTwistAcrossVertical` **Success**이고 `twist total` 줄의 소요 시간이 **0.07초 이하**, peak가 2880 deg/s 부근. `FacingIsContinuous`·`BoneAnglesAreContinuous`·`UpVectorStaysUpright` 전부 **Success**.

- [ ] **Step 7: 새 단언이 실제로 무는지 확인(규약 4)** — `FishActor.h`의 `SteepBeginSin` 기본값을 잠시 `0.0f`로 바꿔 빌드하면, 수평 heading에서도 빠른 비틀림이 허용되므로 `FacingIsContinuous`의 twist 단언은 통과하지만 **`UpVectorStaysUpright`가 빨간불이 되어야 한다**(비틀림이 완만한 heading으로 새어 나와 배를 위로 돌린다).

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/float SteepBeginSin = 0.70f;/float SteepBeginSin = 0.0f;/' unreal/Aquarium/Source/Aquarium/FishActor.h
for pass in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium.Fish.UpVectorStaysUpright; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "lowest up vector|Test Completed"
```
기대: **Fail**, `up.Z`가 음수로 찍힌다. 확인 후 되돌린다:

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/float SteepBeginSin = 0.0f;/float SteepBeginSin = 0.70f;/' unreal/Aquarium/Source/Aquarium/FishActor.h
for pass in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"; done
```
**만약 이 단계에서 `UpVectorStaysUpright`가 통과해 버리면** 이 테스트는 여전히 이 결함을 잡지 못하는 것이므로, 되돌린 뒤 이 사실을 후속 항목에 적고 `FacingHasNoLongTwistAcrossVertical`을 유일한 방어선으로 명시한다.

- [ ] **Step 8: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/FishActor.h unreal/Aquarium/Source/Aquarium/FishActor.cpp unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp && git commit -m "$(cat <<'EOF'
fix: 수직 전환 시 0.33초 피루엣 제거 (M2부터의 이월 항목)

원인은 슬루 속도가 아니라 좌표 프레임의 위상이었다. 수직을 사이에 둔
두 heading의 목표 프레임이 전방축 180도 비틀림만큼 다르고, 그것을
540도/초로 소비해 0.333초가 걸렸다. 회전을 swing과 twist로 분해해
swing은 기존 한도를 그대로 지키고 twist만 가파른 구간에서 2880도/초로
태운다. 몸통 굽힘 입력도 swing에서만 뽑는다.
FacingIsContinuous는 두 축을 각각 단언하도록 재작성했고 기대값은
규칙 계층 함수에서 파생한다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 6: 무리 행동 — 이웃 스냅샷 서브시스템과 물고기 결합

**Files:** Create `unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.h`, Create `unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.cpp`, Modify `unreal/Aquarium/Source/Aquarium/FishActor.h`, Modify `unreal/Aquarium/Source/Aquarium/FishActor.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 1: `FishSchoolSubsystem.h` 생성**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <vector>

#include "aquarium/Boids.h"

#include "FishSchoolSubsystem.generated.h"

class AFishActor;

// Supplies every fish with the two things the rules layer cannot produce on its own: the list of
// other fish, and (from Task 8) the props that intersect its swim plane. It owns no behaviour --
// separation, alignment, cohesion and obstacle steering all live in aquarium::Boids /
// aquarium::Obstacles, where Catch2 can reach them.
UCLASS()
class AQUARIUM_API UFishSchoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void Register(AFishActor* Fish);
	void Unregister(AFishActor* Fish);
	int32 RegisteredCount() const { return Fishes.Num(); }

	// Every registered fish in the shared swim frame. Rebuilt at most once per frame.
	const std::vector<aquarium::BoidNeighbor>& Neighbors();

	// Dev-only per-item toggles for the performance attribution run. Public and plain bools so a
	// test can set them directly without going through the command line.
	bool bSchoolingEnabled = true;
	bool bPropAvoidanceEnabled = true;
	// Pure, so it is testable without a command line. Returns true when the flag is present.
	static bool ParseDisableFlag(const TCHAR* CmdLine, const TCHAR* Flag);

	// Test hook: forces the next Neighbors() call to rebuild.
	void InvalidateSnapshot() { bSnapshotValid = false; }

private:
	TArray<TWeakObjectPtr<AFishActor>> Fishes;
	std::vector<aquarium::BoidNeighbor> Snapshot;
	uint64 SnapshotFrame = 0;
	bool bSnapshotValid = false;
};
```

- [ ] **Step 2: `FishSchoolSubsystem.cpp` 생성**

```cpp
#include "FishSchoolSubsystem.h"

#include "FishActor.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UFishSchoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
	// Per-item toggles, so the performance run can attribute cost to ONE behaviour at a time.
	// This is the only method that survived M4b: the design-stage prediction that the translucent
	// curtains would be the expensive item was refuted by exactly this kind of measurement.
	bSchoolingEnabled = !ParseDisableFlag(FCommandLine::Get(), TEXT("AquariumNoSchooling"));
	bPropAvoidanceEnabled = !ParseDisableFlag(FCommandLine::Get(), TEXT("AquariumNoPropAvoid"));
#endif
}

bool UFishSchoolSubsystem::ParseDisableFlag(const TCHAR* CmdLine, const TCHAR* Flag)
{
	if (CmdLine == nullptr || Flag == nullptr)
	{
		return false;
	}
	return FParse::Param(CmdLine, Flag);
}

void UFishSchoolSubsystem::Register(AFishActor* Fish)
{
	if (Fish == nullptr)
	{
		return;
	}
	Fishes.AddUnique(Fish);
	bSnapshotValid = false;
}

void UFishSchoolSubsystem::Unregister(AFishActor* Fish)
{
	Fishes.RemoveAll([Fish](const TWeakObjectPtr<AFishActor>& P) { return !P.IsValid() || P.Get() == Fish; });
	bSnapshotValid = false;
}

const std::vector<aquarium::BoidNeighbor>& UFishSchoolSubsystem::Neighbors()
{
	// Rebuilt lazily by whichever fish asks first in a frame. A UTickableWorldSubsystem has no
	// ordering guarantee against actor ticks, so half the school would read a one-frame-stale
	// snapshot; this way every fish in a frame sees the same current data. The order is
	// registration order, which keeps AFishActor's determinism test meaningful.
	const uint64 Frame = GFrameCounter;
	if (bSnapshotValid && SnapshotFrame == Frame)
	{
		return Snapshot;
	}
	Fishes.RemoveAll([](const TWeakObjectPtr<AFishActor>& P) { return !P.IsValid(); });
	Snapshot.clear();
	Snapshot.reserve(static_cast<size_t>(Fishes.Num()));
	for (const TWeakObjectPtr<AFishActor>& P : Fishes)
	{
		Snapshot.push_back(P->AsNeighbor());
	}
	SnapshotFrame = Frame;
	bSnapshotValid = true;
	return Snapshot;
}
```

- [ ] **Step 3: `FishActor.h` — 무리 관련 선언 추가**

include 목록에 추가:

```cpp
#include "aquarium/Boids.h"
```

public 프로퍼티에 추가(`SteepBeginSin` 다음):

```cpp
	// How much of a background fish's desired direction comes from its school rather than its own
	// wander target. Not 1.0 on purpose: at 1.0 a whole species congeals into one block, and the
	// remaining wander is what keeps the group loose. Tune this from the clip, not from a still.
	UPROPERTY(EditAnywhere, Category = "Swim", meta = (ClampMin = "0", ClampMax = "1")) float SchoolWeight = 0.55f;
```

public 메서드에 추가:

```cpp
	// This fish as the rules layer sees it, in the shared swim frame (x = world Y, y = world Z).
	aquarium::BoidNeighbor AsNeighbor() const;
	// Equality key derived from the mesh asset. Never logged, never stored.
	int32 SpeciesKey() const { return SpeciesKeyValue; }
```

`virtual void BeginPlay() override;` 다음에 추가:

```cpp
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
```

private 멤버에 추가:

```cpp
	aquarium::BoidsParams BoidsParamsValue;
	int32 SpeciesKeyValue = 0;
	int32 ComputeSpeciesKey() const;
```

- [ ] **Step 4: `FishActor.cpp` — 구현**

include에 추가:

```cpp
#include "FishSchoolSubsystem.h"
#include "Engine/World.h"
```

`InitializeSwim` 안, `FacingParamsValue...` 다음에 추가:

```cpp
	SpeciesKeyValue = ComputeSpeciesKey();
```

파일 끝(`Tick` 앞)에 추가:

```cpp
int32 AFishActor::ComputeSpeciesKey() const
{
	// Derived from the mesh asset rather than authored as its own property. A species id that the
	// level script would also have to write is the same duplicated-rule trap that let a prop
	// radius bug live in BOTH build_reef_m1.py and verify_scene.py until M4b, where the verifier
	// could never catch it. A fish with no mesh (test spawns) gets 0 and schools with other
	// mesh-less fish; the tests pin that explicitly rather than leaving it to chance.
	return FishMesh ? static_cast<int32>(GetTypeHash(FishMesh->GetFName())) : 0;
}

aquarium::BoidNeighbor AFishActor::AsNeighbor() const
{
	aquarium::BoidNeighbor N;
	// Shared frame: every swim plane uses right = +Y and up = +Z, so adding the plane origin back
	// gives one common 2D frame that any fish can use a direction from without conversion.
	N.position = {Plane.origin.y + Motion.position.x, Plane.origin.z + Motion.position.y};
	N.velocity = Motion.velocity;
	N.depth = Plane.origin.x;
	N.species = SpeciesKeyValue;
	// The player's fish is a neighbour to be avoided, never one to be followed. Attracting the
	// school to it would crowd exactly the fish the child is watching; ignoring it entirely would
	// let other species swim through its body.
	N.avoidOnly = bIsPlayerFish;
	return N;
}

void AFishActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* W = GetWorld())
	{
		if (UFishSchoolSubsystem* School = W->GetSubsystem<UFishSchoolSubsystem>())
		{
			School->Unregister(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}
```

`BeginPlay`를 다음으로 바꾼다:

```cpp
void AFishActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeSwim();
	if (UWorld* W = GetWorld())
	{
		if (UFishSchoolSubsystem* School = W->GetSubsystem<UFishSchoolSubsystem>())
		{
			School->Register(this);
		}
	}
}
```

`StepSwim` 안에서 `const aquarium::Vec2 Desired = ...` 줄을 다음으로 바꾼다:

```cpp
	aquarium::Vec2 Desired = bPlayerControlled ? InputDirection : Wander->DesiredDirection(Motion.position);
	// Schooling applies to background fish only. The player's fish is never a boid: arrow keys
	// must map to motion with nothing mixed in, or the child gets "I pressed left and it went
	// somewhere else" (F-05/F-07). The other direction -- background fish reacting to the player
	// -- is handled inside AsNeighbor(), which marks the player fish avoidOnly.
	if (!bPlayerControlled && !bIsPlayerFish)
	{
		UWorld* W = GetWorld();
		UFishSchoolSubsystem* School = W ? W->GetSubsystem<UFishSchoolSubsystem>() : nullptr;
		if (School && School->bSchoolingEnabled && School->RegisteredCount() > 1)
		{
			const std::vector<aquarium::BoidNeighbor>& N = School->Neighbors();
			const aquarium::Vec2 Shared{Plane.origin.y + Motion.position.x, Plane.origin.z + Motion.position.y};
			const aquarium::BoidsResult R = aquarium::SchoolingSteer(
				Shared, Plane.origin.x, SpeciesKeyValue, N.data(), N.size(), BoidsParamsValue);
			Desired = aquarium::BlendSteering(Desired, R.steer, SchoolWeight);
		}
	}
```

- [ ] **Step 5 (RED → GREEN): Automation 테스트 4개 추가** — `FishActorTests.cpp` 끝에 붙인다. include에 `#include "FishSchoolSubsystem.h"`를 추가한다.

```cpp
namespace
{
// Registers a fish with the world's school subsystem and parks it at a fixed plane position.
AFishActor* SchoolFish(UWorld* World, uint32 Seed, const FVector& Origin, USkeletalMesh* Mesh)
{
	AFishActor* Fish = SpawnFish(World, Seed);
	Fish->PlaneOrigin = Origin;
	Fish->PlaneHalfWidth = 400.f;
	Fish->PlaneHalfHeight = 400.f;
	Fish->FishMesh = Mesh;
	Fish->InitializeSwim();
	World->GetSubsystem<UFishSchoolSubsystem>()->Register(Fish);
	return Fish;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorSchoolMatesPullTogether, "Aquarium.Fish.SchoolMatesPullTogether",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorSchoolMatesPullTogether::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	USkeletalMesh* Tang = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Tang)) return false;

	// Three blue tangs on the same plane, 120 cm apart: inside neighborRadius, outside separation.
	AFishActor* A = SchoolFish(World, 3u, FVector(400.f, -120.f, 150.f), Tang);
	AFishActor* B = SchoolFish(World, 4u, FVector(400.f, 0.f, 150.f), Tang);
	AFishActor* C = SchoolFish(World, 5u, FVector(400.f, 120.f, 150.f), Tang);
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	TestEqual(TEXT("three fish registered"), School->RegisteredCount(), 3);

	const float SpreadBefore = FMath::Abs(A->GetActorLocation().Y - C->GetActorLocation().Y);
	for (int i = 0; i < 400; ++i)
	{
		School->InvalidateSnapshot();
		A->StepSwim(0.05f);
		B->StepSwim(0.05f);
		C->StepSwim(0.05f);
	}
	const float SpreadAfter = FMath::Abs(A->GetActorLocation().Y - C->GetActorLocation().Y);
	AddInfo(FString::Printf(TEXT("outer spread %.1f -> %.1f cm"), SpreadBefore, SpreadAfter));
	// Cohesion must hold the group inside roughly a neighbour radius. Without schooling the three
	// wander targets are independent and the spread runs to the 800 cm plane width.
	return TestTrue(FString::Printf(TEXT("school stayed together (spread %.1f cm, limit 400)"), SpreadAfter), SpreadAfter < 400.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorOtherSpeciesDoNotPull, "Aquarium.Fish.OtherSpeciesDoNotPull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorOtherSpeciesDoNotPull::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	USkeletalMesh* Tang = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	USkeletalMesh* Clown = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/Clownfish/SK_Clownfish.SK_Clownfish"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Tang)) return false;
	if (!TestNotNull(TEXT("SK_Clownfish loads"), Clown)) return false;

	AFishActor* A = SchoolFish(World, 3u, FVector(400.f, 0.f, 150.f), Tang);
	AFishActor* B = SchoolFish(World, 4u, FVector(400.f, 100.f, 150.f), Clown);
	TestNotEqual(TEXT("species keys differ"), A->SpeciesKey(), B->SpeciesKey());

	// A blue tang 100 cm from a clownfish is inside neighborRadius but outside separationRadius,
	// so the clownfish must contribute exactly nothing to the tang's steer.
	const aquarium::BoidNeighbor N = B->AsNeighbor();
	aquarium::BoidsParams P;
	const aquarium::BoidsResult R = aquarium::SchoolingSteer(A->AsNeighbor().position, A->AsNeighbor().depth,
		A->SpeciesKey(), &N, 1, P);
	TestEqual(TEXT("no alignment/cohesion from another species"), R.consideredCount, 0);
	TestEqual(TEXT("no separation at 100 cm from another species"), R.avoidCount, 0);
	// But at 20 cm it IS separated from: a clownfish must not swim through a blue tang.
	B->PlaneOrigin = FVector(400.f, 20.f, 150.f);
	B->InitializeSwim();
	const aquarium::BoidNeighbor Close = B->AsNeighbor();
	const aquarium::BoidsResult R2 = aquarium::SchoolingSteer(A->AsNeighbor().position, A->AsNeighbor().depth,
		A->SpeciesKey(), &Close, 1, P);
	TestEqual(TEXT("separation from another species at 20 cm"), R2.avoidCount, 1);
	return TestTrue(TEXT("pushed away from the other species"), R2.steer.x < -0.9f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorPlayerFishIsAvoidedNotFollowed, "Aquarium.Fish.PlayerFishIsAvoidedNotFollowed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorPlayerFishIsAvoidedNotFollowed::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	USkeletalMesh* Tang = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Tang)) return false;

	AFishActor* Background = SchoolFish(World, 3u, FVector(400.f, 0.f, 150.f), Tang);
	AFishActor* Player = SchoolFish(World, 4u, FVector(400.f, 90.f, 150.f), Tang);  // SAME species
	Player->bIsPlayerFish = true;

	const aquarium::BoidNeighbor N = Player->AsNeighbor();
	TestTrue(TEXT("the player fish is marked avoidOnly"), N.avoidOnly);
	aquarium::BoidsParams P;
	const aquarium::BoidsResult R = aquarium::SchoolingSteer(Background->AsNeighbor().position,
		Background->AsNeighbor().depth, Background->SpeciesKey(), &N, 1, P);
	TestEqual(TEXT("never a cohesion/alignment target, even same species"), R.consideredCount, 0);
	TestEqual(TEXT("avoided at 90 cm (avoidOnlyRadius 110)"), R.avoidCount, 1);
	return TestTrue(TEXT("the school opens away from the player fish"), R.steer.x < -0.9f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorPlayerFishIgnoresSchooling, "Aquarium.Fish.PlayerFishIgnoresSchooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorPlayerFishIgnoresSchooling::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	USkeletalMesh* Tang = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Tang)) return false;

	// A crowd of same-species fish sits hard to the LEFT of the player fish. If the player fish
	// were a boid at all, cohesion would bend it left. It must go exactly where the key says.
	AFishActor* Player = SchoolFish(World, 9u, FVector(400.f, 0.f, 150.f), Tang);
	Player->bIsPlayerFish = true;
	Player->bPlayerControlled = true;
	Player->SetInputDirection(FVector2D(1.f, 0.f));   // screen right
	for (int i = 0; i < 6; ++i)
	{
		SchoolFish(World, 20u + static_cast<uint32>(i), FVector(400.f, -100.f - 15.f * i, 150.f), Tang);
	}
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	const FVector Before = Player->GetActorLocation();
	for (int i = 0; i < 40; ++i)
	{
		School->InvalidateSnapshot();
		Player->StepSwim(0.05f);
	}
	const FVector After = Player->GetActorLocation();
	TestTrue(TEXT("moved right as instructed"), After.Y - Before.Y > 5.f);
	return TestTrue(FString::Printf(TEXT("did not drift vertically (dz = %.3f)"), After.Z - Before.Z),
		FMath::Abs(After.Z - Before.Z) < 2.f);
}
```

- [ ] **Step 6: 빨간불 확인(규약 4)** — 구현을 넣기 전에는 컴파일이 안 되므로, 구현을 넣은 **뒤에** `SchoolWeight` 기본값을 잠시 `0.f`로 바꿔 `SchoolMatesPullTogether`가 실제로 실패하는지 본다.

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/float SchoolWeight = 0.55f;/float SchoolWeight = 0.0f;/' unreal/Aquarium/Source/Aquarium/FishActor.h
for pass in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium.Fish.SchoolMatesPullTogether; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "outer spread|Test Completed"
```
기대: **Fail**, `outer spread`가 400 cm를 넘는다. 이 숫자를 기록한다 — 한도 400이 실제 결함 값과 충분히 떨어져 있는지 확인하는 단계다(규약 5). 여유가 20 % 미만이면 한도를 다시 잡는다. 되돌린다:

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/float SchoolWeight = 0.0f;/float SchoolWeight = 0.55f;/' unreal/Aquarium/Source/Aquarium/FishActor.h
```

- [ ] **Step 7: 빌드 두 번 + `Aquarium.Fish` 전체 실행**

```bash
cd /Users/hans/dev/aquarium
for pass in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: **43** (기존 38 + 진단 1 + 무리 4). 숫자가 43이 아니면 실패한 테스트 이름을 `grep "Result={Fail}"`로 찾아 해결한 뒤 진행한다.

- [ ] **Step 8: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.h unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.cpp unreal/Aquarium/Source/Aquarium/FishActor.h unreal/Aquarium/Source/Aquarium/FishActor.cpp unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp && git commit -m "$(cat <<'EOF'
feat: 배경 물고기 무리 행동 결합 (종별, 플레이어 제외)

UFishSchoolSubsystem이 프레임당 한 번 이웃 스냅샷을 만들어 규칙 계층
보이즈에 넘긴다. 종 키는 메시 에셋에서 파생하며 별도 프로퍼티를 두지
않는다. 플레이어 물고기는 무리 규칙을 전혀 받지 않고, 반대로 배경
물고기에게는 넓은 반경의 회피 전용 이웃이다. Automation 38 -> 43.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 7: 소품에 액터 태그를 붙이고 검증기가 그것을 단언하게 한다

**Files:** Modify `unreal/Aquarium/Scripts/build_reef_m1.py`, Modify `unreal/Aquarium/Scripts/verify_scene.py`

프롭 배치 로직·시드·개수는 **절대 바꾸지 않는다.** 바꾸면 M4b와의 전/후 비교가 성립하지 않는다. 태그 한 줄만 추가한다.

- [ ] **Step 1: `build_reef_m1.py` — 프롭에 태그 추가**

`prop.set_actor_scale3d(unreal.Vector(scale, scale, scale * z_stretch))` 바로 다음 줄에 추가:

```python
    # Runtime obstacle avoidance finds props by this tag and derives their radius from the actor's
    # own bounds. Actor tags survive cooking (actor LABELS are editor-only), and deriving the
    # radius at runtime is deliberate: PROP_HALF_EXTENTS above is a THIRD place the same number
    # could live, and M4b proved that a rule duplicated in two places is a rule the verifier can
    # never check.
    prop.tags = [unreal.Name("AquariumProp")]
```

`REEF_OK` print 문을 다음으로 바꾼다:

```python
tagged = len([a for a in eas.get_all_level_actors() if a.actor_has_tag(unreal.Name("AquariumProp"))])
assert les.save_current_level(), "save_current_level failed"
print("REEF_OK actors=%d fish=%d props=%d tagged=%d dof=%s map=%s"
      % (len(eas.get_all_level_actors()), fish_count, prop_count, tagged,
         "off" if DOF is None else "on", MAP))
```

(기존 `assert les.save_current_level(...)` 줄은 위 블록으로 옮겨졌으므로 중복되지 않게 한 번만 남긴다.)

- [ ] **Step 2: `verify_scene.py` — 태그 수를 단언**

프롭 검사 절에 추가한다(반경 계산 방식은 **그대로 둔다** — 실제 메시 바운드에서 계산한다):

```python
tagged = [a for a in actors if a.actor_has_tag(unreal.Name("AquariumProp"))]
assert len(tagged) == len(props), \
    "prop tag count %d != prop count %d (runtime obstacle avoidance would silently see fewer props)" \
    % (len(tagged), len(props))
```

`SCENE_OK` 출력에 `tagged=%d`를 추가한다.

- [ ] **Step 3: 레벨 재생성과 검증** — **`-FullStdOutLogOutput` 없이 돌리면 아무 출력도 없이 레벨만 덮어쓴다. 종료 코드는 무관한 `GameFeatureData` 오류로 항상 1이다. `*_OK` grep으로만 판정한다.**

```bash
cd /Users/hans/dev/aquarium
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
"$UE" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/build_reef_m1.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "REEF_OK|REEF_WARN|Traceback|LogPython: Error"
"$UE" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/verify_scene.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "SCENE_OK|Traceback|LogPython: Error|AssertionError"
```
기대: `REEF_OK ... props=22 tagged=22 ...`와 `SCENE_OK ... tagged=22 ...`.

- [ ] **Step 4: 검증기가 실제로 무는지 확인(규약 6)** — `build_reef_m1.py`의 태그 줄을 잠시 주석 처리하고 레벨을 다시 만든 뒤 `verify_scene.py`를 돌린다.

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/^    prop.tags = \[unreal.Name("AquariumProp")\]/    # prop.tags = [unreal.Name("AquariumProp")]/' unreal/Aquarium/Scripts/build_reef_m1.py
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
"$UE" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/build_reef_m1.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "REEF_OK"
"$UE" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/verify_scene.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "SCENE_OK|AssertionError"
```
기대: `REEF_OK ... tagged=0`, 그리고 `verify_scene.py`가 `AssertionError: prop tag count 0 != prop count 22`. 확인 후 되돌리고 레벨을 다시 만든다:

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/^    # prop.tags = \[unreal.Name("AquariumProp")\]/    prop.tags = [unreal.Name("AquariumProp")]/' unreal/Aquarium/Scripts/build_reef_m1.py
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
"$UE" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/build_reef_m1.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "REEF_OK"
```
기대: `REEF_OK ... props=22 tagged=22`.

- [ ] **Step 5: 커밋** (레벨 `.umap`은 LFS 대상이다. `git lfs status`로 포인터가 맞는지 한 번 본다.)

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Scripts/build_reef_m1.py unreal/Aquarium/Scripts/verify_scene.py unreal/Aquarium/Content/Maps && git lfs status | head -10 && git commit -m "$(cat <<'EOF'
feat: 프롭에 AquariumProp 액터 태그 추가와 검증 단언

런타임 소품 회피가 액터 태그로 프롭을 찾고 반경은 액터 바운드에서
파생한다. PROP_HALF_EXTENTS 표를 C++로 베끼지 않기 위해서다.
verify_scene.py가 태그 수와 프롭 수의 일치를 단언하므로 태그가 빠지면
런타임에 조용히 죽는 대신 레벨 빌드 단계에서 걸린다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 8: 소품 회피 결합 — 액터 바운드에서 원판 파생

**Files:** Modify `unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.h`, Modify `unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.cpp`, Modify `unreal/Aquarium/Source/Aquarium/FishActor.h`, Modify `unreal/Aquarium/Source/Aquarium/FishActor.cpp`, Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 1: 서브시스템에 장애물 API 추가** — `FishSchoolSubsystem.h`의 include에 `#include "aquarium/Obstacles.h"`를 추가하고, public에 다음을 넣는다.

```cpp
	// Obstacle discs for one swim plane, in that plane's LOCAL 2D coordinates. Derived from the
	// bounds of the actors tagged AquariumProp -- never from a radius table copied into C++.
	// PlaneHalfDepth is how far along world X a prop may be and still matter to this plane.
	void BuildObstaclesForPlane(const FVector& PlaneOrigin, float PlaneHalfDepth,
	                            std::vector<aquarium::Obstacle>& Out);
	// Name of the actor tag build_reef_m1.py puts on every prop.
	static const FName PropTag;
	// Cap on how many discs one prop is approximated by (see the .cpp for why a prop is a stack).
	static constexpr int32 MaxDiscsPerProp = 4;
```

- [ ] **Step 2: `FishSchoolSubsystem.cpp`에 구현 추가** — include에 `#include "EngineUtils.h"`, `#include "Engine/StaticMeshActor.h"`를 추가한다.

```cpp
const FName UFishSchoolSubsystem::PropTag(TEXT("AquariumProp"));

void UFishSchoolSubsystem::BuildObstaclesForPlane(const FVector& PlaneOrigin, float PlaneHalfDepth,
                                                  std::vector<aquarium::Obstacle>& Out)
{
	Out.clear();
	UWorld* W = GetWorld();
	if (W == nullptr)
	{
		return;
	}
	for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
	{
		AStaticMeshActor* Prop = *It;
		if (Prop == nullptr || !Prop->ActorHasTag(PropTag))
		{
			continue;
		}
		// The actor's own bounds already include its mesh, its scale and its tilt. That is the
		// point: the radius is DERIVED here, so it cannot drift away from what is on screen the
		// way a copied table would.
		const FBox Box = Prop->GetComponentsBoundingBox(/*bNonColliding*/ true);
		if (!Box.IsValid)
		{
			continue;
		}
		const FVector Centre = Box.GetCenter();
		const FVector Extent = Box.GetExtent();
		// Depth gate: a prop only matters to this plane if it actually reaches it along world X.
		if (FMath::Abs(Centre.X - PlaneOrigin.X) > Extent.X + PlaneHalfDepth)
		{
			continue;
		}
		// A prop is approximated by a STACK of discs rather than one disc. One disc forces a bad
		// choice: a radius equal to the half width leaves a tall coral's top and bottom open,
		// while a radius equal to the half height turns a 60 cm coral into a 150 cm wall that
		// fish swerve around from far away. A stack matches the silhouette and costs a few more
		// cheap circle tests.
		const float RadiusCm = FMath::Max(static_cast<float>(Extent.Y), 1.f);
		const int32 Discs = FMath::Clamp(FMath::CeilToInt(static_cast<float>(Extent.Z) / RadiusCm), 1, MaxDiscsPerProp);
		const float Span = 2.f * static_cast<float>(Extent.Z);
		for (int32 i = 0; i < Discs; ++i)
		{
			const float T = (Discs == 1) ? 0.5f : (static_cast<float>(i) + 0.5f) / static_cast<float>(Discs);
			const float WorldZ = static_cast<float>(Centre.Z - Extent.Z) + Span * T;
			aquarium::Obstacle O;
			// Plane-local: the fish's own 2D position is relative to its plane origin.
			O.center = {static_cast<float>(Centre.Y) - static_cast<float>(PlaneOrigin.Y),
			            WorldZ - static_cast<float>(PlaneOrigin.Z)};
			O.radius = RadiusCm;
			Out.push_back(O);
		}
	}
}
```

- [ ] **Step 3: `FishActor.h` — 장애물 보관과 파라미터**

include에 `#include "aquarium/Obstacles.h"`를 추가하고, private 멤버에 다음을 넣는다.

```cpp
	// Built ONCE in InitializeSwim. Every fish's plane X is fixed for its whole life, so the
	// "which props reach my plane" question has a constant answer; only a handful of discs
	// survive, which is why per-tick obstacle cost is a few circle tests and never 22.
	std::vector<aquarium::Obstacle> PlaneObstacles;
	aquarium::ObstacleParams ObstacleParamsValue;
```

public 프로퍼티에 추가:

```cpp
	// How far along world X a prop may be and still count as intersecting this fish's plane.
	UPROPERTY(EditAnywhere, Category = "Swim") float ObstaclePlaneHalfDepth = 40.f;
```

- [ ] **Step 4: `FishActor.cpp` — 초기화와 조향 결합**

`InitializeSwim`의 `SpeciesKeyValue = ComputeSpeciesKey();` 다음에 추가:

```cpp
	PlaneObstacles.clear();
	if (UWorld* W = GetWorld())
	{
		if (UFishSchoolSubsystem* School = W->GetSubsystem<UFishSchoolSubsystem>())
		{
			School->BuildObstaclesForPlane(PlaneOrigin, ObstaclePlaneHalfDepth, PlaneObstacles);
		}
	}
```

`StepSwim`에서 경계 규칙 줄(`const aquarium::Vec2 Dir = bPlayerControlled ? ...`) **앞에** 다음을 넣는다:

```cpp
	// Props, before the boundary rule so the wall always gets the last word: obstacle avoidance
	// must never be able to push a fish out of its plane (M3 pinned "no escape at any aspect
	// ratio"). Like SteerAlongBoundary, this preserves the magnitude of the desired direction --
	// killing the blocked component would let the velocity decelerate through zero and flip the
	// facing 180 degrees, which is the bug M3 had to fix once already.
	if (!PlaneObstacles.empty())
	{
		UWorld* WObs = GetWorld();
		UFishSchoolSubsystem* SchoolObs = WObs ? WObs->GetSubsystem<UFishSchoolSubsystem>() : nullptr;
		if (SchoolObs == nullptr || SchoolObs->bPropAvoidanceEnabled)
		{
			Desired = aquarium::SteerAroundObstacles(Motion.position, Desired, PlaneObstacles.data(),
			                                         PlaneObstacles.size(), ObstacleParamsValue);
		}
	}
```

- [ ] **Step 5 (RED → GREEN): Automation 테스트 3개 추가** — `FishActorTests.cpp` 끝에 붙인다. include에 `#include "Engine/StaticMeshActor.h"`를 추가한다.

```cpp
namespace
{
// Spawns a tagged box prop at a world location, sized like a coral.
AStaticMeshActor* SpawnProp(UWorld* World, const FVector& Location, const FVector& Scale)
{
	AStaticMeshActor* Prop = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, FRotator::ZeroRotator);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Prop->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Prop->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Prop->SetActorScale3D(Scale);
	Prop->Tags.Add(UFishSchoolSubsystem::PropTag);
	return Prop;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorObstaclesDerivedFromPropBounds, "Aquarium.Fish.ObstaclesDerivedFromPropBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorObstaclesDerivedFromPropBounds::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	// /Engine/BasicShapes/Cube is 100 cm, so scale 1 gives half extents of 50 cm.
	SpawnProp(World, FVector(400.f, 100.f, 60.f), FVector(1.f, 1.f, 3.f));   // reaches the plane
	SpawnProp(World, FVector(900.f, 100.f, 60.f), FVector(1.f, 1.f, 1.f));   // 5 m away in depth

	std::vector<aquarium::Obstacle> Obs;
	School->BuildObstaclesForPlane(FVector(400.f, 0.f, 150.f), 40.f, Obs);
	TestTrue(TEXT("the distant prop is filtered out by depth"), Obs.size() <= static_cast<size_t>(UFishSchoolSubsystem::MaxDiscsPerProp));
	if (!TestTrue(TEXT("the near prop produced discs"), !Obs.empty())) return false;
	// Radius is the HALF WIDTH of the actual bounds (50 cm), derived, not a copied table value.
	TestTrue(FString::Printf(TEXT("radius %.1f is the actor's half width"), Obs[0].radius), FMath::IsNearlyEqual(Obs[0].radius, 50.f, 1.f));
	// A 150 cm half-height prop against a 50 cm radius asks for 3 discs.
	TestEqual(TEXT("a tall prop is a stack, not one fat disc"), static_cast<int32>(Obs.size()), 3);
	// Plane-local: the prop is at world Y = 100 and the plane origin at Y = 0.
	return TestTrue(FString::Printf(TEXT("disc centre x %.1f is plane-local"), Obs[0].center.x), FMath::IsNearlyEqual(Obs[0].center.x, 100.f, 1.f));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorSwimsAroundProp, "Aquarium.Fish.SwimsAroundPropInsteadOfThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorSwimsAroundProp::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	SpawnProp(World, FVector(400.f, 0.f, 150.f), FVector(1.f, 1.f, 1.f));   // dead ahead, r = 50

	AFishActor* Fish = SpawnFish(World, 5u);
	Fish->bPlayerControlled = true;
	Fish->PlaneOrigin = FVector(400.f, -250.f, 150.f);
	Fish->PlaneHalfWidth = 400.f;
	Fish->PlaneHalfHeight = 200.f;
	Fish->InitializeSwim();
	Fish->SetInputDirection(FVector2D(1.f, 0.f));   // straight at the prop

	float MinDist = 1e9f;
	for (int i = 0; i < 400; ++i)
	{
		Fish->StepSwim(0.05f);
		const FVector L = Fish->GetActorLocation();
		MinDist = FMath::Min(MinDist, static_cast<float>(FVector2D(L.Y - 0.f, L.Z - 150.f).Size()));
	}
	AddInfo(FString::Printf(TEXT("closest approach %.1f cm to a 50 cm prop"), MinDist));
	// Clearance, not contact: the fish must turn BEFORE it arrives. 50 cm radius, and the rule
	// adds a 20 cm margin, so anything under 50 means it went through the solid part.
	return TestTrue(FString::Printf(TEXT("kept clear of the prop (closest %.1f cm, limit 50)"), MinDist), MinDist > 50.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorSpeedSurvivesPropAvoidance, "Aquarium.Fish.SpeedSurvivesPropAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorSpeedSurvivesPropAvoidance::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	SpawnProp(World, FVector(400.f, 0.f, 150.f), FVector(1.f, 1.f, 1.f));

	AFishActor* Fish = SpawnFish(World, 5u);
	Fish->bPlayerControlled = true;
	Fish->PlaneOrigin = FVector(400.f, -250.f, 150.f);
	Fish->PlaneHalfWidth = 400.f;
	Fish->PlaneHalfHeight = 200.f;
	Fish->InitializeSwim();
	Fish->SetInputDirection(FVector2D(1.f, 0.f));
	for (int i = 0; i < 60; ++i) Fish->StepSwim(0.05f);   // reach cruising speed

	// This is the M3 regression guard: an avoidance rule that zeroes the blocked component makes
	// the speed dip toward zero, and a velocity through zero has no stable heading, which flipped
	// the facing 180 degrees. The speed must stay up the whole way past the prop.
	float MinSpeed = 1e9f;
	float MaxFacingStepDeg = 0.f;
	FQuat Prev = Fish->GetActorQuat();
	for (int i = 0; i < 340; ++i)
	{
		Fish->StepSwim(0.05f);
		MinSpeed = FMath::Min(MinSpeed, Fish->CurrentSpeed());
		const FQuat Now = Fish->GetActorQuat();
		MaxFacingStepDeg = FMath::Max(MaxFacingStepDeg, FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(Prev.GetAxisX(), Now.GetAxisX())), -1.f, 1.f))));
		Prev = Now;
	}
	AddInfo(FString::Printf(TEXT("min speed %.1f cm/s (max %.1f), largest nose swing %.1f deg"), MinSpeed, Fish->MaxSpeed, MaxFacingStepDeg));
	TestTrue(FString::Printf(TEXT("speed never collapsed (min %.1f, floor %.1f)"), MinSpeed, Fish->MaxSpeed * 0.8f), MinSpeed > Fish->MaxSpeed * 0.8f);
	return TestTrue(FString::Printf(TEXT("no facing snap (largest swing %.1f deg)"), MaxFacingStepDeg), MaxFacingStepDeg < 45.f);
}
```

- [ ] **Step 6: 빨간불 확인(규약 4)** — `SteerAroundObstacles` 호출을 잠시 주석 처리해 `SwimsAroundPropInsteadOfThrough`가 실패하는지 본다.

```bash
cd /Users/hans/dev/aquarium && sed -i '' 's/^\t\t\tDesired = aquarium::SteerAroundObstacles/\t\t\t\/\/ Desired = aquarium::SteerAroundObstacles/' unreal/Aquarium/Source/Aquarium/FishActor.cpp
```
빌드 두 번 후 `Automation RunTests Aquarium.Fish.SwimsAroundPropInsteadOfThrough` 실행. 기대: **Fail**, `closest approach`가 0에 가깝다(물고기가 프롭을 그대로 통과). 숫자를 기록한 뒤 주석을 되돌린다(`sed`의 역방향, 또는 해당 두 줄을 직접 복구).

- [ ] **Step 7: 빌드 두 번 + 전체 Automation**

```bash
cd /Users/hans/dev/aquarium
for pass in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: **46** (43 + 소품 3).

- [ ] **Step 8: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.h unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.cpp unreal/Aquarium/Source/Aquarium/FishActor.h unreal/Aquarium/Source/Aquarium/FishActor.cpp unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp && git commit -m "$(cat <<'EOF'
feat: 소품 회피 — 액터 바운드에서 파생한 원판 스택

프롭은 태그로 찾고 반경은 액터 바운드에서 계산한다. 평면 X가 물고기마다
고정이므로 어떤 프롭이 내 평면에 닿는지는 초기화 때 한 번만 푼다.
키 큰 산호는 원판 하나가 아니라 최대 4장의 스택으로 근사해 실루엣에
맞춘다. 조향은 경계 규칙보다 먼저 걸려 벽이 항상 이긴다.
Automation 43 -> 46.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 9: 개발 전용 토글 플래그 테스트

**Files:** Modify `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

`ParseDisableFlag`는 Task 6에서 이미 구현했다. 여기서는 그것이 실제로 검사되는지 못 박는다 — M4b의 "통과하지만 아무것도 검사하지 않던 테스트"를 반복하지 않기 위해서다.

- [ ] **Step 1: 테스트 추가**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishSchoolDevTogglesParse, "Aquarium.Fish.DevTogglesParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishSchoolDevTogglesParse::RunTest(const FString&)
{
	// Pure parser, so no world and no command line are needed. These two flags exist for the
	// per-item performance attribution run: M4b showed that predicting which item is expensive
	// does not work, and toggling one at a time does.
	TestTrue(TEXT("recognises -AquariumNoSchooling"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumNoSchooling -other"), TEXT("AquariumNoSchooling")));
	TestTrue(TEXT("recognises -AquariumNoPropAvoid"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumNoPropAvoid"), TEXT("AquariumNoPropAvoid")));
	TestFalse(TEXT("absent flag is false"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumAutoInput=RRLL"), TEXT("AquariumNoSchooling")));
	TestFalse(TEXT("the two flags are independent"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumNoSchooling"), TEXT("AquariumNoPropAvoid")));
	TestFalse(TEXT("a null command line is false, not a crash"),
		UFishSchoolSubsystem::ParseDisableFlag(nullptr, TEXT("AquariumNoSchooling")));
	return TestFalse(TEXT("a null flag is false, not a crash"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumNoSchooling"), nullptr));
}
```

- [ ] **Step 2: 빨간불 확인** — `ParseDisableFlag`의 본문을 잠시 `return false;`로 바꾸면 앞의 두 단언이 실패해야 한다. 확인 후 되돌린다.

- [ ] **Step 3: 빌드 두 번 + 전체 Automation**

```bash
cd /Users/hans/dev/aquarium
for pass in 1 2; do "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"; done
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
```
기대: **47**.

- [ ] **Step 4: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp && git commit -m "$(cat <<'EOF'
test: 개발 전용 토글 플래그 파서 테스트 추가

-AquariumNoSchooling / -AquariumNoPropAvoid. 항목별 성능 분해에 쓰며
셰이핑 빌드에는 들어가지 않는다. Automation 46 -> 47.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 10: 전체 회귀 — 규칙 90 + Automation 47 + 실제 RHI 로그

**Files:** 없음 (검증만)

- [ ] **Step 1: 규칙 계층 클린 빌드와 전체 테스트**

```bash
cd /Users/hans/dev/aquarium && rm -rf build && cmake -S . -B build && cmake --build build -j 2>&1 | grep -E "warning|error" ; ctest --test-dir build 2>&1 | tail -3
```
기대: 경고 0줄, `100% tests passed, 0 tests failed out of 90`.

- [ ] **Step 2: Automation 전체와 실패 목록 확인**

```bash
cd /Users/hans/dev/aquarium
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | tee /tmp/m4c-auto.log | grep -cE "Test Completed. Result=\{Success\}"
grep -E "Result=\{Fail\}" /tmp/m4c-auto.log || echo "no failures"
```
기대: `47`, `no failures`.

- [ ] **Step 3: 실제 렌더링 경로(Metal RHI) 로그 확인 — `-nullrhi`가 아니어야 한다**

```bash
cd /Users/hans/dev/aquarium
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor" "$PWD/unreal/Aquarium/Aquarium.uproject" ReefM1 -game -benchmark -fps=30 -seconds=12 -windowed -ResX=1280 -ResY=720 -unattended -nosplash -stdout -FullStdOutLogOutput > /tmp/m4c-game.log 2>&1 || true
grep -cE "Failed to compile Material|Sampler type|Default Material|WorldGridMaterial" /tmp/m4c-game.log
wc -l /tmp/m4c-game.log
```
기대: 첫 숫자 **0**. 0이 아니면 그 자리에서 멈추고 원인을 찾는다 — M4a에서 이 부류를 두 번 다 **캡처를 눈으로 보고서야** 발견했다.

---

### Task 11: 캡처 — 클립이 1차 산출물이다

**Files:** Create `scripts/render_m4c_compare.sh`

M4c의 세 항목(무리, 소품 회피, 롤 제거)은 **정지 화면에 나타나지 않는다.** 스틸은 M4b 회귀 확인용이고, 판정은 클립으로 한다.

- [ ] **Step 1: `scripts/render_m4c_compare.sh` 작성** — `scripts/render_m4b_compare.sh`를 복사해 아래 네 가지를 바꾼다. 나머지(빌드 두 번, 맵·시드·카메라·자동 입력 패턴, ffmpeg 인자)는 **그대로 둔다** — 비교가 성립하려면 같아야 한다.

```bash
cp /Users/hans/dev/aquarium/scripts/render_m4b_compare.sh /Users/hans/dev/aquarium/scripts/render_m4c_compare.sh
chmod +x /Users/hans/dev/aquarium/scripts/render_m4c_compare.sh
```

바꿀 네 가지:

1. 모든 `m4b` 산출물 이름을 `m4c`로 바꾸되, **`BEFORE` 기본값은 `docs/reviews/2026-09-21-m4b-scene.png`로 둔다.** M4b에서 이름 일괄 변경이 `BEFORE`까지 바꿔 버려 M4b를 M4b와 비교한 사고가 있었다. 변경 후 반드시 확인한다:

```bash
grep -n "BEFORE" /Users/hans/dev/aquarium/scripts/render_m4c_compare.sh
```
기대: `2026-09-21-m4b-scene.png`가 보여야 한다. `m4c`가 보이면 잘못된 것이다.

2. 산호 근접 스틸(`-coral.png`) 단계를 **삭제한다.** M4c는 산호를 건드리지 않으므로 M4b 스틸의 재촬영일 뿐이다.

3. 메인 클립 길이를 20초 → **45초**로 늘린다(`-seconds=` 인자와 ffmpeg 길이). 무리 형성은 20초 안에 충분히 일어나지 않는다.

4. **수직 전환 전용 클립**을 추가한다. 위 → 위오른쪽 → 위 → 위왼쪽을 반복하는 자동 입력으로 12초를 찍어 `docs/reviews/<날짜>-m4c-vertical.mp4`로 저장한다. 롤 결함은 이 전환에서만 나타나므로 이것이 "고쳐졌다"의 유일한 시각 증거다.

```bash
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor"
"$UE" "$PROJ" ReefM1 -game -benchmark -fps=60 -seconds=12 \
  -AquariumAutoNickname="측정" -AquariumAssignmentSeed=1 \
  -AquariumAutoInput="U:1.5,UR:1.5,U:1.5,UL:1.5" \
  -ResX=1920 -ResY=1080 -windowed -unattended -nosplash \
  -stdout -FullStdOutLogOutput > "$LOG_VERTICAL" 2>&1 || true
```
(패턴 토큰 표기는 `ADiverPlayerController::BuildAutoInputSteps`가 받는 형식을 **코드에서 확인해** 맞춘다. 추측하지 않는다 — 인식하지 못한 토큰은 경고만 찍고 조용히 무시되므로, `AquariumAutoInput: unknown direction` 경고가 로그에 없는지 grep으로 확인한다.)

- [ ] **Step 2: 실행**

```bash
cd /Users/hans/dev/aquarium && ./scripts/render_m4c_compare.sh 2>&1 | tail -20
ls -l docs/reviews/$(date +%F)-m4c-*
```
기대: `-m4c-reef.mp4`(45초), `-m4c-vertical.mp4`(12초), `-m4c-scene.png`, `-m4c-compare.png`.

- [ ] **Step 3: 머티리얼 로그 단언**

```bash
grep -cE "Failed to compile Material" /tmp/m4c-*.log
grep -c "AquariumAutoInput: unknown direction" /tmp/m4c-*.log
```
기대: 전부 0.

- [ ] **Step 4: 산출물을 직접 본다 — 이 단계는 건너뛸 수 없다**
  - `-m4c-vertical.mp4`: 물고기가 위로 방향을 바꿀 때 **제자리 회전이 보이지 않는가.** Task 0에서 본 `before-reef.mp4`와 비교한다.
  - `-m4c-reef.mp4`: (1) 같은 종이 느슨한 무리로 몰려다니는가, 한 덩어리로 굳지 않았는가, (2) 물고기가 산호·바위를 관통하지 않고 **앞에서 미리** 도는가, (3) 소품을 피할 때 속력이 죽거나 방향이 뒤집히지 않는가, (4) 배경 물고기가 플레이어 물고기를 둘러싸지 않고 길을 비키는가.
  - `-m4c-compare.png`: M4b의 빛줄기·안개·산호 색이 **그대로인가**(회귀 없음).
  - 무리가 너무 뭉치거나 너무 흩어져 보이면 `SchoolWeight`(0.55)와 `BoidsParams::cohesionWeight`를 조정하고 **클립을 다시 찍는다.** 스틸로 판단하지 않는다.

- [ ] **Step 5: 커밋** (mp4·png는 LFS)

```bash
cd /Users/hans/dev/aquarium && git add scripts/render_m4c_compare.sh docs/reviews/$(date +%F)-m4c-* && git lfs status | head -10 && git commit -m "$(cat <<'EOF'
chore: M4c 비교 산출물 하네스와 캡처

무리·소품 회피·수직 전환은 전부 시간 축 현상이라 클립이 1차 산출물이다.
45초 산호초 클립과 수직 전환 전용 12초 클립을 찍고, 스틸은 M4b 회귀
확인용으로만 나란히 비교한다. BEFORE 기본값은 M4b 스틸로 고정했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 12: 성능 재측정과 항목별 분해

**Files:** Create `docs/reviews/<날짜>-m4c-perf.md`

기준선: M4b 평균 **63.8 fps / p95 16.58 ms**(1920×1080 에디터 빌드). SRS 게이트는 60 fps / 22 ms. **실행 간 노이즈는 약 1.3 fps이므로 1.3 fps 미만 차이는 신호로 해석하지 않는다.**

- [ ] **Step 1: 기본 설정 측정 2회**

```bash
cd /Users/hans/dev/aquarium && ./scripts/measure_m2b_perf.sh 2>&1 | tail -5
```
두 번 돌려 평균 fps가 서로 1.3 fps 안에 드는지 확인한다. 들지 않으면 배경 프로세스를 정리하고 다시 잰다.

- [ ] **Step 2: 항목별 토글 — 한 번에 하나만 끈다**

```bash
cd /Users/hans/dev/aquarium
for FLAG in "-AquariumNoSchooling" "-AquariumNoPropAvoid" "-AquariumNoSchooling -AquariumNoPropAvoid"; do
  echo "=== $FLAG ==="
  EXTRA_ARGS="$FLAG" ./scripts/measure_m2b_perf.sh 2>&1 | tail -3
done
```
(`measure_m2b_perf.sh`가 `EXTRA_ARGS`를 명령줄에 전달하지 않으면 **한 줄 추가한다.** M4b의 `AQ_*` 환경변수 토글과 같은 규약이다.)

- [ ] **Step 3: 보고서 작성** — `docs/reviews/<날짜>-m4c-perf.md`에 M4b 보고서와 같은 형식으로 쓴다. 반드시 포함할 것:
  - 4가지 설정의 평균 fps / p95 ms 표
  - M4b 기준선 대비 차이와, **그 차이가 1.3 fps 노이즈 바닥 위인지 아래인지 한 줄로 판정**
  - 무리와 소품 회피 각각의 귀속 비용
  - 평균이 60 fps 미만이면 컷 목록에서 적용한 항목과 재측정 결과
  - `GridSizeZ` 128 → 64 레버를 **쓰지 않았다는 사실과 그 이유**(안 썼다면)

- [ ] **Step 4: 판정**
  - 평균 ≥ 60 fps: 그대로 간다. 레버는 M5/M6용으로 남긴다.
  - 평균 < 60 fps: 설계 문서의 컷 목록 **순서대로** 하나씩 되돌리고 **되돌릴 때마다 재측정**한다. 한꺼번에 여러 개를 되돌리면 무엇이 비쌌는지 영원히 모른다.
  - 보이즈가 노이즈 바닥 위로 뚜렷하게 올라왔다면(예: 3 fps 이상) 컷 목록 1·2번보다 **균일 격자 도입**을 먼저 검토한다. 화질을 깎기 전에 알고리즘을 고치는 것이 순서다.

- [ ] **Step 5: 커밋**

```bash
cd /Users/hans/dev/aquarium && git add docs/reviews/$(date +%F)-m4c-perf.md docs/reviews/$(date +%F)-m2b-frametimes.csv scripts/measure_m2b_perf.sh && git commit -m "$(cat <<'EOF'
perf: M4c 프레임 시간 재측정과 항목별 비용 분해

무리·소품 회피를 각각 끄고 네 가지 설정으로 측정했다. 1.3 fps 노이즈
바닥을 기준으로 신호 여부를 판정한다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

---

### Task 13: 문서 갱신과 마무리

**Files:** Modify `docs/TASK.md`, Modify `docs/SETUP.md`, Modify this plan

- [ ] **Step 1: `docs/TASK.md` 갱신**
  - 로드맵 표의 M4c 행을 `**열려 있음**`에서 M4a/M4b와 같은 형식의 구현 완료 기록으로 바꾼다: 규칙 **90** + Automation **47**, 측정한 평균 fps / p95, 산출물 경로, `**사용자 시각 검토: 대기**`.
  - "검증 현황"의 규칙 테스트 개수 66 → 90, Automation 38 → 47로 갱신하고 신규 테스트를 한 줄로 요약한다.
  - **M2 이후 이월되어 온 "수직으로 방향을 바꿀 때 짧은 롤(0.3초)" 항목을 해소로 표시하고, 실제 원인(전방축 180도 비틀림)과 측정값을 한 줄로 적는다.** M2b의 "소품 충돌" 이월 항목도 같이 해소 표시한다.
  - 상단 문단의 "M4c는 열려 있다"를 갱신한다.

- [ ] **Step 2: `docs/SETUP.md` 갱신** — 개발 전용 플래그 목록에 `-AquariumNoSchooling`, `-AquariumNoPropAvoid`를 추가하고 용도(항목별 성능 분해)를 적는다.

- [ ] **Step 3: 이 계획 문서의 "구현 중 발견한 후속 항목" 절을 채운다** — 아래 빈 절에 실제로 발견한 것을 적는다. 최소한 다음은 반드시 기록한다.
  - Task 4에서 **측정한 비틀림 각도와 소요 시간**(진단의 근거).
  - Task 6 Step 6, Task 8 Step 6에서 **빨간불일 때의 실제 수치**와 한도의 여유.
  - `SchoolWeight`·`cohesionWeight`를 클립을 보고 조정했다면 최종값과 이유.
  - Step 7(Task 5)에서 `UpVectorStaysUpright`가 빨간불이 되지 않았다면 그 사실.

- [ ] **Step 4: 자체 검토** — 커밋 전에 세 가지를 본다.
  1. **사양 대응**: 설계 문서의 결정 표 모든 행에 대응하는 태스크가 있는가.
  2. **자리표시자 검사**: `grep -rn "TBD\|TODO\|FIXME\|적절히\|비슷하게" docs/superpowers/plans/2026-09-21-m4c-schooling.md rules/ unreal/Aquarium/Source/` 결과가 비어 있는가.
  3. **이름·시그니처 일관성**: `SchoolingSteer`, `SteerAroundObstacles`, `MaxTwistStepDeg`, `AsNeighbor`, `BuildObstaclesForPlane`, `PropTag`가 헤더·구현·테스트·계획에서 전부 같은 철자와 인자 목록인가.

- [ ] **Step 5: 최종 검증 후 커밋**

```bash
cd /Users/hans/dev/aquarium
ctest --test-dir build 2>&1 | tail -2
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -cE "Test Completed. Result=\{Success\}"
git status --short
```
기대: `100% tests passed ... out of 90`, `47`.

```bash
cd /Users/hans/dev/aquarium && git add docs/TASK.md docs/SETUP.md docs/superpowers/plans/2026-09-21-m4c-schooling.md && git commit -m "$(cat <<'EOF'
docs: M4c 결과 기록 (무리 행동, 소품 회피, 수직 롤 제거)

규칙 66 -> 90, Automation 38 -> 47. M2부터 이월된 수직 전환 롤과
M2b부터 이월된 소품 충돌을 해소로 표시했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01574CFGe3fEWvWY7mPHqLKJ
EOF
)"
```

- [ ] **Step 6: 사용자 시각 검토 요청** — M4a·M4b와 같은 형식으로, 클립 두 개(`-m4c-reef.mp4`, `-m4c-vertical.mp4`)와 비교 스틸 하나를 제시하고 **"클립이 판정 대상이고 스틸은 회귀 확인용"** 임을 명시한다. 알려진 품질 한계를 숨기지 않고 함께 적는다.

---

## 구현 중 발견한 후속 항목

*(실행 중 채운다. 비워 둔 채로 M4c를 닫지 않는다.)*

- **동종만 무리 짓는다는 것을 시각적으로 완전히 확정하지 못했다** — `2026-09-21-m4c-reef.mp4`에서
  블루탱이 무리를 이루고 헤딩이 맞는 것은 분명히 보이지만, 프레임 안에 서로 다른 종이 가까이 있는
  경우가 남아 있고 배경 물고기는 화면에서 작다. **분리는 설계상 모든 종에 걸쳐 적용하므로 어느 정도의
  섞임은 정상이다.** 즉 눈으로는 설계대로인 것과 결함인 것을 구분할 수 없었다. 규칙 계층 테스트와
  Automation 테스트는 동종 판정을 직접 고정하고 있으므로 기능적으로는 덮여 있지만, **시각 확인은
  미완이다.** 확정하려면 한 종만 스폰하거나 종별로 색을 입힌 디버그 뷰가 필요하다.
- **`SpeedSurvivesPropAvoidance`의 여유가 약 15%뿐이다** — 실측 37.0 cm/s 대 한도 32.0 cm/s.
  시뮬레이션이 결정론적이라 flaky하지는 않지만, 앞으로 `MaxSpeed`나 `Accel`을 손보면 이 테스트가
  먼저 깨질 수 있다. 깨지면 튜닝을 되돌리라는 신호가 아니라 **한도를 그 값에 맞춰 다시 유도하라는
  신호**다(한도는 리터럴이 아니라 규칙 계층 상수에서 유도해야 한다).
- **소품 회피 전/후 비교는 프레임 정렬되어 있지 않다** — 회피를 켜면 경로 자체가 갈라지므로
  같은 프레임 번호가 같은 순간을 뜻하지 않는다. 대신 확인한 것은 **진입 지점과 타이밍이 일치하고,
  갈라짐이 정확히 장애물 위치에서 시작한다**는 점이다. 프레임 단위로 겹쳐 보는 비교가 필요하면
  회피 강도를 0으로 둔 별도 캡처 모드를 만들어야 한다.
- **성능 여유가 SRS 게이트 대비 약 5.4%로 얇다** — M4b의 약 6%와 같은 수준이고 M4c가 더한 비용은
  측정에 잡히지 않았다. `GridSizeZ` 128 → 64 레버(+17.1 fps)는 **M5·M6을 위해 일부러 아껴 두었다.**
  기본값으로 미리 써 버리면 이후 단계의 실제 비용이 영구히 가려진다. 상세는
  [`2026-09-21-m4c-perf.md`](../../reviews/2026-09-21-m4c-perf.md).

- **M4b에서 이월(그대로 열려 있음)** — TubeCoral 말뚝 울타리 실루엣, FanCoral이 BranchCoral과
  구분되지 않음, PlateCoral·BrainCoral의 흰 덩어리(`CORAL_TINTS[0]`이 항등 틴트), 스카이라이트
  상향에 따른 바닥 밝기, `GOBO_COARSE_THRESHOLD`와 빛줄기 선명도의 절충. 전부 산호를 다시
  만드는 일이라 M4c 범위 밖이다. 상세는 [M4b 구현 계획](2026-09-21-m4b-reef-lighting.md)의 같은 절.
- **M4a에서 이월(그대로 열려 있음)** — 비늘 이방성 셀, 가슴지느러미 위치·크기, 나비고기만
  `pecStray=16`, 고정 바운드의 `BoundsScale`, 미사용 `SK_*_PhysicsAsset`.
- **M3에서 이월** — F-06(포커스 상실 일시정지)의 실제 윈도 포커스 델리게이트는 헤드리스에서
  발생하지 않는다. 창 모드 수동 확인 1회가 여전히 필요하다.
- **`PropMaterialsCompile`의 남은 한계** — 셰이더 컴파일 오류는 `-nullrhi`에 `FMaterialResource`가
  없어 헤드리스에서 검사되지 않는다. 1차 방어선은 여전히 실제 게임 실행 로그 grep이다.
