# 규칙 계층(엔진 독립 C++) 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Unreal 없이 clang+CMake만으로 테스트 가능한 아쿠아리움 게임 규칙 라이브러리(`aquarium_rules`)를 TDD로 완성한다. SRS F-01~F-14의 규칙 부분을 결정적 단위 테스트로 검증한다.

**Architecture:** 순수 C++17 정적 라이브러리. 렌더링·입력 이벤트·시간원(clock)에 의존하지 않고, 난수와 시간 간격(dt)을 모두 주입받는다. 이후 Unreal 프로젝트가 이 소스를 그대로 포함해 사용한다(별도 복제 금지 — SRS 구조 원칙).

**Tech Stack:** C++17, CMake ≥ 3.24, Catch2 v3 (FetchContent), Apple clang(arm64).

**전제:** 작업 디렉터리 `/Users/hans/dev/aquarium`, 브랜치는 `feat/rules-layer`를 새로 만든다. 각 태스크의 테스트 실행 명령은 공통이다:

```bash
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
```

**파일 구조(최종):**

```
CMakeLists.txt
rules/
  include/aquarium/
    Vec2.h            # 2D 벡터 유틸
    Nickname.h        # F-01 별명 검증
    FishCatalog.h     # F-03 무작위 배정(난수 주입)
    Session.h         # F-02, F-14 세션 수명
    Steering.h        # F-05 방향키 → 입력 벡터
    Motion.h          # F-06 가속/감속/일시정지/dt 안전
    Bounds.h          # F-07 경계 회피와 안전 제한
    Flee.h            # F-10, F-11, F-12 도망 상태 기계
    Wander.h          # F-08 자율 유영(시드 결정적)
  src/
    Nickname.cpp
    (그 외는 헤더 온리)
tests/
  test_vec2.cpp  test_nickname.cpp  test_catalog.cpp  test_session.cpp
  test_steering.cpp  test_motion.cpp  test_bounds.cpp  test_flee.cpp  test_wander.cpp
```

---

### Task 0: 브랜치와 빌드 스캐폴드

**Files:**
- Create: `CMakeLists.txt`
- Create: `tests/test_vec2.cpp` (임시 스모크 1개 포함)

- [ ] **Step 1: 브랜치 생성**

```bash
git checkout -b feat/rules-layer
```

- [ ] **Step 2: CMakeLists.txt 작성**

```cmake
cmake_minimum_required(VERSION 3.24)
project(aquarium_rules LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(aquarium_rules
  rules/src/Nickname.cpp
)
target_include_directories(aquarium_rules PUBLIC rules/include)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.4
)
FetchContent_MakeAvailable(Catch2)

enable_testing()
add_executable(rules_tests
  tests/test_vec2.cpp
)
target_link_libraries(rules_tests PRIVATE aquarium_rules Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(rules_tests)
```

주의: `rules/src/Nickname.cpp`는 Task 2에서 생기므로, Task 0 시점에는 임시로 빈 파일을 만들어 둔다:

```bash
mkdir -p rules/src rules/include/aquarium tests
printf '// filled in Task 2\n' > rules/src/Nickname.cpp
```

- [ ] **Step 3: 스모크 테스트 작성** — `tests/test_vec2.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("build scaffold works") {
    REQUIRE(1 + 1 == 2);
}
```

- [ ] **Step 4: 빌드와 테스트 실행**

Run: 공통 명령. Expected: `100% tests passed, 1 test`.

- [ ] **Step 5: 커밋**

```bash
git add CMakeLists.txt rules tests
git commit -m "chore: CMake + Catch2 test scaffold for rules layer"
```

---

### Task 1: Vec2 유틸

**Files:**
- Create: `rules/include/aquarium/Vec2.h`
- Modify: `tests/test_vec2.cpp`

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_vec2.cpp` 전체 교체

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Vec2.h"

using aquarium::Vec2;
using Catch::Approx;

TEST_CASE("length of 3-4 vector is 5") {
    REQUIRE(Vec2{3.f, 4.f}.Length() == Approx(5.f));
}

TEST_CASE("Normalized keeps direction, unit length") {
    Vec2 n = Vec2{10.f, 0.f}.Normalized();
    REQUIRE(n.x == Approx(1.f));
    REQUIRE(n.y == Approx(0.f));
}

TEST_CASE("Normalized of zero vector is zero, not NaN") {
    Vec2 n = Vec2{0.f, 0.f}.Normalized();
    REQUIRE(n.x == 0.f);
    REQUIRE(n.y == 0.f);
}

TEST_CASE("arithmetic operators") {
    Vec2 v = Vec2{1.f, 2.f} + Vec2{3.f, 4.f} * 2.f;
    REQUIRE(v.x == Approx(7.f));
    REQUIRE(v.y == Approx(10.f));
}
```

- [ ] **Step 2: 실패 확인** — Expected: 컴파일 실패 `'aquarium/Vec2.h' file not found`. (컴파일 오류도 RED로 기록하되, 헤더 생성 후 의도한 동작 실패/통과를 다시 확인한다.)

- [ ] **Step 3: 구현** — `rules/include/aquarium/Vec2.h`

```cpp
#pragma once
#include <cmath>

namespace aquarium {

struct Vec2 {
    float x = 0.f;
    float y = 0.f;

    float Length() const { return std::sqrt(x * x + y * y); }

    Vec2 Normalized() const {
        const float len = Length();
        if (len <= 1e-6f) return {0.f, 0.f};
        return {x / len, y / len};
    }

    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Vec2.h tests/test_vec2.cpp
git commit -m "feat: Vec2 utility with zero-safe normalization"
```

---

### Task 2: 별명 검증 (F-01)

**Files:**
- Create: `rules/include/aquarium/Nickname.h`
- Modify: `rules/src/Nickname.cpp` (Task 0의 빈 파일 교체)
- Create: `tests/test_nickname.cpp` — CMake `rules_tests` 소스에 추가

문자 수 규칙: UTF-8 **코드포인트** 기준 최대 12자로 구현한다. NFC로 조합된 한글 1글자는 코드포인트 1개다. ZWJ 이모지 시퀀스는 코드포인트 여러 개로 세어져 더 엄격하게 제한되는데, 이는 허용된 근사치로 문서화한다(진짜 grapheme 계산은 Unreal 계층 검증 항목으로 이월). 제어 문자(U+0000–U+001F, U+007F)와 줄바꿈은 거부한다. 앞뒤 공백(ASCII space/tab)은 제거한다.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_nickname.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "aquarium/Nickname.h"

using namespace aquarium;

TEST_CASE("trims surrounding spaces") {
    auto r = ValidateNickname("  nemo  ");
    REQUIRE(r.ok);
    REQUIRE(r.value == "nemo");
}

TEST_CASE("rejects empty and whitespace-only") {
    REQUIRE_FALSE(ValidateNickname("").ok);
    REQUIRE_FALSE(ValidateNickname("   ").ok);
    REQUIRE(ValidateNickname("   ").error == NicknameError::Empty);
}

TEST_CASE("rejects control chars and newlines") {
    REQUIRE_FALSE(ValidateNickname("ne\nmo").ok);
    REQUIRE_FALSE(ValidateNickname("ne\tmo").ok);   // interior tab
    REQUIRE_FALSE(ValidateNickname(std::string("a\x01b", 3)).ok);
    REQUIRE(ValidateNickname("ne\nmo").error == NicknameError::InvalidCharacter);
}

TEST_CASE("hangul counts by codepoint") {
    // 12 hangul syllables = 12 codepoints -> ok
    REQUIRE(ValidateNickname(u8"가나다라마바사아자차카타").ok);
    // 13 -> too long
    auto r = ValidateNickname(u8"가나다라마바사아자차카타파");
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.error == NicknameError::TooLong);
}

TEST_CASE("13 ascii chars rejected, 12 accepted") {
    REQUIRE(ValidateNickname("abcdefghijkl").ok);
    REQUIRE_FALSE(ValidateNickname("abcdefghijklm").ok);
}
```

CMakeLists.txt의 `add_executable(rules_tests ...)`에 `tests/test_nickname.cpp` 추가.

- [ ] **Step 2: 실패 확인** — Expected: 헤더 없음 컴파일 실패 → 헤더/스텁 작성 후 assertion FAIL 확인.

- [ ] **Step 3: 구현**

`rules/include/aquarium/Nickname.h`:

```cpp
#pragma once
#include <string>

namespace aquarium {

enum class NicknameError { None, Empty, TooLong, InvalidCharacter };

struct NicknameResult {
    bool ok = false;
    std::string value;                     // trimmed nickname when ok
    NicknameError error = NicknameError::None;
};

inline constexpr int kMaxNicknameCodepoints = 12;

NicknameResult ValidateNickname(const std::string& utf8);

} // namespace aquarium
```

`rules/src/Nickname.cpp`:

```cpp
#include "aquarium/Nickname.h"

namespace aquarium {
namespace {

bool IsAsciiSpace(unsigned char c) { return c == ' '; }
bool IsControl(unsigned char c) { return c < 0x20 || c == 0x7F; }

// Counts UTF-8 codepoints; returns -1 on invalid leading byte pattern.
int CountCodepoints(const std::string& s) {
    int count = 0;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t adv = 0;
        if (c < 0x80) adv = 1;
        else if ((c >> 5) == 0x6) adv = 2;
        else if ((c >> 4) == 0xE) adv = 3;
        else if ((c >> 3) == 0x1E) adv = 4;
        else return -1;
        i += adv;
        if (i > s.size()) return -1;
        ++count;
    }
    return count;
}

} // namespace

NicknameResult ValidateNickname(const std::string& utf8) {
    size_t begin = 0, end = utf8.size();
    while (begin < end && IsAsciiSpace(static_cast<unsigned char>(utf8[begin]))) ++begin;
    while (end > begin && IsAsciiSpace(static_cast<unsigned char>(utf8[end - 1]))) --end;
    const std::string trimmed = utf8.substr(begin, end - begin);

    if (trimmed.empty()) return {false, "", NicknameError::Empty};

    for (unsigned char c : trimmed) {
        if (IsControl(c)) return {false, "", NicknameError::InvalidCharacter};
    }

    const int cp = CountCodepoints(trimmed);
    if (cp < 0) return {false, "", NicknameError::InvalidCharacter};
    if (cp > kMaxNicknameCodepoints) return {false, "", NicknameError::TooLong};

    return {true, trimmed, NicknameError::None};
}

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Nickname.h rules/src/Nickname.cpp tests/test_nickname.cpp CMakeLists.txt
git commit -m "feat: nickname validation (F-01) with UTF-8 codepoint limit"
```

---

### Task 3: 물고기 무작위 배정 (F-03)

**Files:**
- Create: `rules/include/aquarium/FishCatalog.h`
- Create: `tests/test_catalog.cpp` — CMake 소스에 추가

난수는 `size_t(size_t bound)` 형태의 함수(0 ≤ 반환 < bound)로 주입한다.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_catalog.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "aquarium/FishCatalog.h"

using namespace aquarium;

TEST_CASE("picks first and last item via injected rng") {
    auto first = PickFishIndex(3, [](size_t) { return size_t{0}; });
    auto last  = PickFishIndex(3, [](size_t b) { return b - 1; });
    REQUIRE(first.has_value());
    REQUIRE(*first == 0);
    REQUIRE(*last == 2);
}

TEST_CASE("empty catalog yields no assignment") {
    bool called = false;
    auto r = PickFishIndex(0, [&](size_t) { called = true; return size_t{0}; });
    REQUIRE_FALSE(r.has_value());
    REQUIRE_FALSE(called);   // rng must not run on empty catalog
}

TEST_CASE("out-of-range rng result is clamped into catalog") {
    auto r = PickFishIndex(3, [](size_t) { return size_t{99}; });
    REQUIRE(r.has_value());
    REQUIRE(*r == 2);
}
```

- [ ] **Step 2: 실패 확인** — Expected: 헤더 없음 → 스텁 후 FAIL 확인.

- [ ] **Step 3: 구현** — `rules/include/aquarium/FishCatalog.h`

```cpp
#pragma once
#include <cstddef>
#include <functional>
#include <optional>

namespace aquarium {

using PickFn = std::function<size_t(size_t bound)>; // returns value in [0, bound)

inline std::optional<size_t> PickFishIndex(size_t catalogSize, const PickFn& pick) {
    if (catalogSize == 0) return std::nullopt;
    size_t idx = pick(catalogSize);
    if (idx >= catalogSize) idx = catalogSize - 1;
    return idx;
}

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/FishCatalog.h tests/test_catalog.cpp CMakeLists.txt
git commit -m "feat: random fish assignment with injected rng (F-03)"
```

---

### Task 4: 세션 수명 (F-02, F-14)

**Files:**
- Create: `rules/include/aquarium/Session.h`
- Create: `tests/test_session.cpp` — CMake 소스에 추가

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_session.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "aquarium/Session.h"

using namespace aquarium;

static size_t PickZero(size_t) { return 0; }

TEST_CASE("valid nickname begins a session owning exactly one fish") {
    SessionManager m;
    auto r = m.Begin("nemo", 3, PickZero);
    REQUIRE(r == BeginResult::Ok);
    REQUIRE(m.HasActiveSession());
    REQUIRE(m.Nickname() == "nemo");
    REQUIRE(m.OwnedFishIndex() == 0);
}

TEST_CASE("double begin is rejected (no duplicate session)") {
    SessionManager m;
    REQUIRE(m.Begin("nemo", 3, PickZero) == BeginResult::Ok);
    REQUIRE(m.Begin("dory", 3, PickZero) == BeginResult::AlreadyActive);
    REQUIRE(m.Nickname() == "nemo");   // first session untouched
}

TEST_CASE("invalid nickname or empty catalog rejects entry") {
    SessionManager m;
    REQUIRE(m.Begin("   ", 3, PickZero) == BeginResult::InvalidNickname);
    REQUIRE(m.Begin("nemo", 0, PickZero) == BeginResult::EmptyCatalog);
    REQUIRE_FALSE(m.HasActiveSession());
}

TEST_CASE("End clears nickname and ownership; re-entry is fresh (F-14)") {
    SessionManager m;
    m.Begin("nemo", 3, PickZero);
    m.End();
    REQUIRE_FALSE(m.HasActiveSession());
    REQUIRE(m.Nickname().empty());
    REQUIRE(m.Begin("dory", 3, [](size_t) { return size_t{2}; }) == BeginResult::Ok);
    REQUIRE(m.Nickname() == "dory");
    REQUIRE(m.OwnedFishIndex() == 2);
}
```

- [ ] **Step 2: 실패 확인** — Expected: 헤더 없음 → 스텁 후 FAIL 확인.

- [ ] **Step 3: 구현** — `rules/include/aquarium/Session.h`

```cpp
#pragma once
#include <string>

#include "aquarium/FishCatalog.h"
#include "aquarium/Nickname.h"

namespace aquarium {

enum class BeginResult { Ok, InvalidNickname, EmptyCatalog, AlreadyActive };

class SessionManager {
public:
    BeginResult Begin(const std::string& rawNickname, size_t catalogSize, const PickFn& pick) {
        if (active_) return BeginResult::AlreadyActive;
        const NicknameResult n = ValidateNickname(rawNickname);
        if (!n.ok) return BeginResult::InvalidNickname;
        const auto idx = PickFishIndex(catalogSize, pick);
        if (!idx) return BeginResult::EmptyCatalog;
        nickname_ = n.value;
        ownedFishIndex_ = *idx;
        active_ = true;
        return BeginResult::Ok;
    }

    void End() {
        active_ = false;
        nickname_.clear();
        ownedFishIndex_ = 0;
    }

    bool HasActiveSession() const { return active_; }
    const std::string& Nickname() const { return nickname_; }
    size_t OwnedFishIndex() const { return ownedFishIndex_; }

private:
    bool active_ = false;
    std::string nickname_;
    size_t ownedFishIndex_ = 0;
};

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Session.h tests/test_session.cpp CMakeLists.txt
git commit -m "feat: session lifecycle with single ownership (F-02, F-14)"
```

---

### Task 5: 방향키 입력 벡터 (F-05)

**Files:**
- Create: `rules/include/aquarium/Steering.h`
- Create: `tests/test_steering.cpp` — CMake 소스에 추가

좌표 규약: 화면 기준 x+ = 오른쪽, y+ = 위. (Unreal 월드 좌표로의 변환은 연동 계층 책임.)

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_steering.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Steering.h"

using namespace aquarium;
using Catch::Approx;

TEST_CASE("single keys map to screen directions") {
    REQUIRE(SteeringVector({true, false, false, false}).y == Approx(1.f));   // up
    REQUIRE(SteeringVector({false, true, false, false}).y == Approx(-1.f));  // down
    REQUIRE(SteeringVector({false, false, true, false}).x == Approx(-1.f));  // left
    REQUIRE(SteeringVector({false, false, false, true}).x == Approx(1.f));   // right
}

TEST_CASE("opposing keys cancel") {
    Vec2 v = SteeringVector({true, true, true, true});
    REQUIRE(v.x == 0.f);
    REQUIRE(v.y == 0.f);
}

TEST_CASE("diagonal is normalized to unit length") {
    Vec2 v = SteeringVector({true, false, false, true});
    REQUIRE(v.Length() == Approx(1.f));
    REQUIRE(v.x > 0.f);
    REQUIRE(v.y > 0.f);
}

TEST_CASE("no keys means zero vector") {
    Vec2 v = SteeringVector({false, false, false, false});
    REQUIRE(v.Length() == 0.f);
}
```

- [ ] **Step 2: 실패 확인** — Expected: 헤더 없음 → 스텁 후 FAIL 확인.

- [ ] **Step 3: 구현** — `rules/include/aquarium/Steering.h`

```cpp
#pragma once
#include "aquarium/Vec2.h"

namespace aquarium {

struct KeyState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

inline Vec2 SteeringVector(KeyState k) {
    Vec2 v{
        (k.right ? 1.f : 0.f) - (k.left ? 1.f : 0.f),
        (k.up ? 1.f : 0.f) - (k.down ? 1.f : 0.f),
    };
    return v.Normalized();
}

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Steering.h tests/test_steering.cpp CMakeLists.txt
git commit -m "feat: arrow-key steering vector (F-05)"
```

---

### Task 6: 이동·감속·dt 안전 (F-06)

**Files:**
- Create: `rules/include/aquarium/Motion.h`
- Create: `tests/test_motion.cpp` — CMake 소스에 추가

규칙: 입력 방향 × maxSpeed가 목표 속도. 현재 속도를 목표 쪽으로 가속률(입력 있음: accel, 없음: decel)만큼 이동. dt ≤ 0이면 아무 변화 없음, dt는 maxDeltaTime으로 상한(포커스 복귀 순간이동 방지). `paused`면 변화 없음.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_motion.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Motion.h"

using namespace aquarium;
using Catch::Approx;

static MotionParams P() {
    MotionParams p;
    p.maxSpeed = 100.f;
    p.accel = 200.f;      // units/s^2
    p.decel = 150.f;
    p.maxDeltaTime = 0.1f;
    return p;
}

TEST_CASE("accelerates toward max speed under input") {
    MotionState s;
    StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.x == Approx(20.f));       // 200 * 0.1
    StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.x == Approx(40.f));
}

TEST_CASE("velocity never exceeds max speed") {
    MotionState s;
    for (int i = 0; i < 100; ++i) StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.Length() <= Approx(100.f));
}

TEST_CASE("decelerates smoothly to zero when input released") {
    MotionState s;
    s.velocity = {30.f, 0.f};
    StepMotion(s, {0.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.x == Approx(15.f));       // 30 - 150*0.1
    StepMotion(s, {0.f, 0.f}, P(), 0.1f);
    REQUIRE(s.velocity.x == 0.f);                // no overshoot past zero
}

TEST_CASE("position integrates velocity") {
    MotionState s;
    s.velocity = {50.f, 0.f};
    StepMotion(s, {0.f, 0.f}, P(), 0.f);         // dt=0: nothing moves
    REQUIRE(s.position.x == 0.f);
    StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.position.x > 0.f);
}

TEST_CASE("huge dt is clamped (no teleport after focus loss)") {
    MotionState a, b;
    StepMotion(a, {1.f, 0.f}, P(), 5.0f);        // clamped to 0.1
    StepMotion(b, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(a.position.x == Approx(b.position.x));
    REQUIRE(a.velocity.x == Approx(b.velocity.x));
}

TEST_CASE("paused simulation does not change state") {
    MotionState s;
    s.velocity = {50.f, 0.f};
    s.paused = true;
    StepMotion(s, {1.f, 0.f}, P(), 0.1f);
    REQUIRE(s.position.x == 0.f);
    REQUIRE(s.velocity.x == Approx(50.f));
}
```

- [ ] **Step 2: 실패 확인** — Expected: 헤더 없음 → 스텁 후 FAIL 확인.

- [ ] **Step 3: 구현** — `rules/include/aquarium/Motion.h`

```cpp
#pragma once
#include <algorithm>

#include "aquarium/Vec2.h"

namespace aquarium {

struct MotionParams {
    float maxSpeed = 100.f;
    float accel = 200.f;
    float decel = 150.f;
    float maxDeltaTime = 0.1f;
};

struct MotionState {
    Vec2 position;
    Vec2 velocity;
    bool paused = false;
};

// Moves current velocity toward target by at most `rate * dt`, then integrates position.
inline void StepMotion(MotionState& s, Vec2 inputDir, const MotionParams& p, float dt) {
    if (s.paused || dt <= 0.f) return;
    dt = std::min(dt, p.maxDeltaTime);

    const bool hasInput = inputDir.Length() > 1e-6f;
    const Vec2 target = inputDir.Normalized() * (hasInput ? p.maxSpeed : 0.f);
    const float rate = (hasInput ? p.accel : p.decel) * dt;

    const Vec2 diff = target - s.velocity;
    const float dist = diff.Length();
    s.velocity = (dist <= rate) ? target : s.velocity + diff.Normalized() * rate;

    if (s.velocity.Length() > p.maxSpeed)
        s.velocity = s.velocity.Normalized() * p.maxSpeed;

    s.position = s.position + s.velocity * dt;
}

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Motion.h tests/test_motion.cpp CMakeLists.txt
git commit -m "feat: motion with smooth accel/decel and dt clamping (F-06)"
```

---

### Task 7: 경계 회피와 안전 제한 (F-07)

**Files:**
- Create: `rules/include/aquarium/Bounds.h`
- Create: `tests/test_bounds.cpp` — CMake 소스에 추가

규칙: 유영 영역(Rect) 경계에 `avoidDistance` 이내로 접근하고 그 경계를 향해 이동 중이면, 이동 방향의 해당 축 성분을 0으로 죽여 회피한다(양 축 독립 처리 — 모서리에서도 동작). 최종 안전장치로 위치를 영역 안으로 강제 클램프한다.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_bounds.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Bounds.h"

using namespace aquarium;
using Catch::Approx;

static const Rect kArea{0.f, 0.f, 100.f, 100.f};

TEST_CASE("direction unchanged in open water") {
    Vec2 d = AvoidBoundary({50.f, 50.f}, {1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == Approx(1.f));
}

TEST_CASE("outward component removed near each edge") {
    REQUIRE(AvoidBoundary({95.f, 50.f}, {1.f, 0.f}, kArea, 10.f).x == 0.f);   // right
    REQUIRE(AvoidBoundary({5.f, 50.f}, {-1.f, 0.f}, kArea, 10.f).x == 0.f);   // left
    REQUIRE(AvoidBoundary({50.f, 95.f}, {0.f, 1.f}, kArea, 10.f).y == 0.f);   // top
    REQUIRE(AvoidBoundary({50.f, 5.f}, {0.f, -1.f}, kArea, 10.f).y == 0.f);   // bottom
}

TEST_CASE("inward movement near edge is not blocked") {
    Vec2 d = AvoidBoundary({95.f, 50.f}, {-1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == Approx(-1.f));
}

TEST_CASE("corner blocks both outward axes") {
    Vec2 d = AvoidBoundary({95.f, 95.f}, Vec2{1.f, 1.f}.Normalized(), kArea, 10.f);
    REQUIRE(d.x == 0.f);
    REQUIRE(d.y == 0.f);
}

TEST_CASE("clamp forces position inside area") {
    Vec2 p = ClampToArea({150.f, -20.f}, kArea);
    REQUIRE(p.x == Approx(100.f));
    REQUIRE(p.y == Approx(0.f));
}
```

- [ ] **Step 2: 실패 확인** — Expected: 헤더 없음 → 스텁 후 FAIL 확인.

- [ ] **Step 3: 구현** — `rules/include/aquarium/Bounds.h`

```cpp
#pragma once
#include <algorithm>

#include "aquarium/Vec2.h"

namespace aquarium {

struct Rect {
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
};

// Kills the outward axis component of `dir` when within avoidDistance of an edge.
inline Vec2 AvoidBoundary(Vec2 pos, Vec2 dir, Rect area, float avoidDistance) {
    if (dir.x > 0.f && pos.x > area.maxX - avoidDistance) dir.x = 0.f;
    if (dir.x < 0.f && pos.x < area.minX + avoidDistance) dir.x = 0.f;
    if (dir.y > 0.f && pos.y > area.maxY - avoidDistance) dir.y = 0.f;
    if (dir.y < 0.f && pos.y < area.minY + avoidDistance) dir.y = 0.f;
    return dir;
}

inline Vec2 ClampToArea(Vec2 pos, Rect area) {
    return {std::clamp(pos.x, area.minX, area.maxX),
            std::clamp(pos.y, area.minY, area.maxY)};
}

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Bounds.h tests/test_bounds.cpp CMakeLists.txt
git commit -m "feat: boundary avoidance and hard clamp (F-07)"
```

---

### Task 8: 도망 상태 기계 (F-10, F-11, F-12)

**Files:**
- Create: `rules/include/aquarium/Flee.h`
- Create: `tests/test_flee.cpp` — CMake 소스에 추가

상태: `Normal → Fleeing(0.8s) → Recovering(1.2s) → Normal`. 도망 중 터치는 무시, 회복 중 터치는 도망 재시작. 도망 방향: 터치점→물고기 방향. 그 방향이 0이면 현재 진행 방향, 진행 방향도 0이면 주입된 대체 방향(화면 중심 쪽). `EffectiveInput()`은 F-12 규칙으로 방향키 입력을 필터링한다: Fleeing이면 도망 방향, Recovering/Normal이면 플레이어 입력.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_flee.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/Flee.h"

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

TEST_CASE("touch during flee is ignored; touch during recover restarts flee") {
    FleeStateMachine m;
    m.Touch({0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);
    m.Step(0.4f);
    m.Touch({0.f, 0.f}, {-10.f, 0.f}, {0.f, 0.f}, {0.f, 1.f}, kP);   // ignored
    REQUIRE(m.FleeDirection().x == Approx(1.f));                      // unchanged
    m.Step(0.4f);                                                     // total 0.8 -> Recovering
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
```

- [ ] **Step 2: 실패 확인** — Expected: 헤더 없음 → 스텁 후 FAIL 확인.

- [ ] **Step 3: 구현** — `rules/include/aquarium/Flee.h`

```cpp
#pragma once
#include "aquarium/Vec2.h"

namespace aquarium {

enum class BehaviorState { Normal, Fleeing, Recovering };

struct FleeParams {
    float fleeDuration = 0.8f;
    float recoverDuration = 1.2f;
};

// Direction away from touch; falls back to current heading, then to fallbackDir.
inline Vec2 ComputeFleeDirection(Vec2 touch, Vec2 fishPos, Vec2 velocity, Vec2 fallbackDir) {
    const Vec2 away = (fishPos - touch).Normalized();
    if (away.Length() > 0.f) return away;
    const Vec2 heading = velocity.Normalized();
    if (heading.Length() > 0.f) return heading;
    return fallbackDir.Normalized();
}

class FleeStateMachine {
public:
    void Touch(Vec2 touch, Vec2 fishPos, Vec2 velocity, Vec2 fallbackDir, const FleeParams& p) {
        if (state_ == BehaviorState::Fleeing) return;   // F-11: ignore re-touch mid-flee
        fleeDir_ = ComputeFleeDirection(touch, fishPos, velocity, fallbackDir);
        state_ = BehaviorState::Fleeing;
        timer_ = p.fleeDuration;
        recoverDuration_ = p.recoverDuration;
    }

    void Step(float dt) {
        if (state_ == BehaviorState::Normal || dt <= 0.f) return;
        timer_ -= dt;
        if (timer_ > 0.f) return;
        if (state_ == BehaviorState::Fleeing) {
            state_ = BehaviorState::Recovering;
            timer_ += recoverDuration_;
            if (timer_ <= 0.f) state_ = BehaviorState::Normal;
        } else {
            state_ = BehaviorState::Normal;
        }
    }

    // F-12: flee overrides player input; recovery restores it.
    Vec2 EffectiveInput(Vec2 playerInput) const {
        return state_ == BehaviorState::Fleeing ? fleeDir_ : playerInput;
    }

    BehaviorState State() const { return state_; }
    Vec2 FleeDirection() const { return fleeDir_; }

private:
    BehaviorState state_ = BehaviorState::Normal;
    Vec2 fleeDir_;
    float timer_ = 0.f;
    float recoverDuration_ = 0.f;
};

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Flee.h tests/test_flee.cpp CMakeLists.txt
git commit -m "feat: flee/recover state machine (F-10, F-11, F-12)"
```

---

### Task 9: 자율 유영 (F-08)

**Files:**
- Create: `rules/include/aquarium/Wander.h`
- Create: `tests/test_wander.cpp` — CMake 소스에 추가

규칙: 각 물고기는 시드로 초기화된 자체 RNG(`std::mt19937`)와 목표점을 갖는다. 목표점에 도달 반경 이내로 접근하거나 목표 유지 시간이 만료되면 유영 영역 안의 새 목표를 뽑는다. `DesiredDirection`은 현재 위치→목표 방향을 반환한다. 같은 시드는 같은 목표 수열(결정적), 다른 시드는 다른 수열.

- [ ] **Step 1: 실패하는 테스트 작성** — `tests/test_wander.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "aquarium/Wander.h"

using namespace aquarium;

static const Rect kArea{0.f, 0.f, 100.f, 100.f};

TEST_CASE("same seed yields identical target sequence") {
    WanderBehavior a(42u, kArea), b(42u, kArea);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(a.Target().x == b.Target().x);
        REQUIRE(a.Target().y == b.Target().y);
        a.ForceNewTarget();
        b.ForceNewTarget();
    }
}

TEST_CASE("different seeds yield different targets (no lockstep, F-08)") {
    WanderBehavior a(1u, kArea), b(2u, kArea);
    bool anyDifferent = false;
    for (int i = 0; i < 5; ++i) {
        if (a.Target().x != b.Target().x || a.Target().y != b.Target().y) anyDifferent = true;
        a.ForceNewTarget();
        b.ForceNewTarget();
    }
    REQUIRE(anyDifferent);
}

TEST_CASE("targets stay inside swim area") {
    WanderBehavior w(7u, kArea);
    for (int i = 0; i < 50; ++i) {
        Vec2 t = w.Target();
        REQUIRE(t.x >= kArea.minX); REQUIRE(t.x <= kArea.maxX);
        REQUIRE(t.y >= kArea.minY); REQUIRE(t.y <= kArea.maxY);
        w.ForceNewTarget();
    }
}

TEST_CASE("reaching target picks a new one") {
    WanderBehavior w(7u, kArea);
    const Vec2 first = w.Target();
    w.Update(first, 0.016f);            // standing on the target
    const Vec2 next = w.Target();
    const bool moved = (next.x != first.x) || (next.y != first.y);
    REQUIRE(moved);
}

TEST_CASE("desired direction points toward target") {
    WanderBehavior w(7u, kArea);
    Vec2 pos{0.f, 0.f};
    Vec2 d = w.DesiredDirection(pos);
    Vec2 expect = (w.Target() - pos).Normalized();
    REQUIRE(d.x == expect.x);
    REQUIRE(d.y == expect.y);
}
```

- [ ] **Step 2: 실패 확인** — Expected: 헤더 없음 → 스텁 후 FAIL 확인.

- [ ] **Step 3: 구현** — `rules/include/aquarium/Wander.h`

```cpp
#pragma once
#include <random>

#include "aquarium/Bounds.h"
#include "aquarium/Vec2.h"

namespace aquarium {

class WanderBehavior {
public:
    WanderBehavior(uint32_t seed, Rect area, float arriveRadius = 2.f, float targetLifetime = 6.f)
        : rng_(seed), area_(area), arriveRadius_(arriveRadius), targetLifetime_(targetLifetime) {
        PickTarget();
    }

    // Call each frame with the fish position; renews target on arrival or timeout.
    void Update(Vec2 pos, float dt) {
        age_ += dt;
        if ((target_ - pos).Length() <= arriveRadius_ || age_ >= targetLifetime_) PickTarget();
    }

    Vec2 DesiredDirection(Vec2 pos) const { return (target_ - pos).Normalized(); }
    Vec2 Target() const { return target_; }
    void ForceNewTarget() { PickTarget(); }

private:
    void PickTarget() {
        std::uniform_real_distribution<float> dx(area_.minX, area_.maxX);
        std::uniform_real_distribution<float> dy(area_.minY, area_.maxY);
        target_ = {dx(rng_), dy(rng_)};
        age_ = 0.f;
    }

    std::mt19937 rng_;
    Rect area_;
    float arriveRadius_;
    float targetLifetime_;
    float age_ = 0.f;
    Vec2 target_;
};

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — 공통 명령. Expected: 전체 PASS.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Wander.h tests/test_wander.cpp CMakeLists.txt
git commit -m "feat: seeded autonomous wander behavior (F-08)"
```

---

### Task 10: 마무리 — 문서 갱신과 푸시

**Files:**
- Modify: `docs/TASK.md` (검증 현황 갱신)

- [ ] **Step 1: 전체 테스트 최종 실행**

Run: 공통 명령. Expected: 모든 테스트 PASS (약 30개 이상).

- [ ] **Step 2: TASK.md 검증 현황 갱신** — "게임 동작 테스트: 미실행" 항목을 규칙 계층 테스트 결과(테스트 수, 커밋 해시)로 교체. M1 행에 "규칙 테스트 환경 완료(Unreal 장면 제외)" 메모 추가.

- [ ] **Step 3: 커밋과 푸시**

```bash
git add docs/TASK.md
git commit -m "docs: record rules-layer test results"
git push -u origin feat/rules-layer
```

- [ ] **Step 4: superpowers:finishing-a-development-branch 스킬로 병합/PR 결정**

---

## 범위 밖 (다음 계획)

- Unreal 연동 계층(입력 이벤트, Actor, 레이캐스트 F-09, 카메라 변환, UI F-04) — **M0 도구 설치 완료 후** 별도 계획.
- F-13 애니메이션 파라미터 연동, 산호초 장면, 성능 측정 — Unreal 계획에 포함.
- 별명의 grapheme 단위 정밀 계산 — Unreal `FString` 계층에서 재검증.
