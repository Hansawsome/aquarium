# M1 — 최소 수중 장면과 블루탱 유영 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Blender 스크립트로 만든 블루탱 한 마리가 규칙 계층의 자율 유영과 절차적 사인파 애니메이션으로 고정 잠수부 시점 수중 장면을 헤엄치는 30초 영상을 만들어 사용자 시각 검토를 받는다.

**Architecture:** 에셋은 전부 스크립트로 재현한다 — Blender는 `blender -b -P` 배치 스크립트, Unreal 콘텐츠는 에디터 Python(`UnrealEditor-Cmd -run=pythonscript`). 게임 로직은 규칙 계층(`rules/`, 순수 C++17, Catch2)에 먼저 넣고 Unreal 모듈이 같은 소스를 컴파일한다. `AFishActor`는 규칙 계층 결과를 월드 좌표와 본 회전으로 옮기는 얇은 껍데기다.

**Tech Stack:** Blender 5.2 (bpy), Unreal 5.8.2 (C++, EditorScriptingUtilities, Python 3.11), CMake + Catch2, ffmpeg.

**전제:** 작업 디렉터리 `/Users/hans/dev/aquarium`. 규칙 계층 테스트 명령(이하 "ctest 명령"):

```bash
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
```

Unreal 에디터 빌드 명령(이하 "UE 빌드"):

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -vi "\[upgrade\]" | grep -E "error|Result|Total execution"
```

Unreal 자동화 테스트 명령(이하 "UE 테스트"):

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "LogAutomationController|Test Completed|Aquarium\." | tail -20
```

에디터 Python 스크립트 실행(이하 "UE 파이썬 `<script>`"):

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/<script>" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "LogPython|Error|Warning: Failed" | tail -30
```

**파일 구조(최종):**

```
assets/blender/
  make_bluetang.py          # 메시·리깅·재질·텍스처 베이크·FBX 내보내기 (배치 실행)
  BlueTang.blend            # 스크립트 산출물 (LFS)
  export/BlueTang.fbx       # 스크립트 산출물 (LFS)
  export/T_BlueTang_BaseColor.png  (LFS)
assets/textures/sand/       # Poly Haven CC0 (LFS), 출처는 docs/ASSETS.md
rules/include/aquarium/
  SwimPlane.h               # 2D 유영 평면 → 3D 월드 좌표 (순수)
  SwimAnimation.h           # 속도·시간 → 본 각도 (순수)
tests/test_swimplane.cpp  tests/test_swimanimation.cpp
unreal/Aquarium/
  Source/Aquarium/Aquarium.Build.cs        # rules/ 포함
  Source/Aquarium/FishActor.h/.cpp         # 규칙 계층 → 액터
  Source/Aquarium/AquariumGameMode.h/.cpp  # 고정 카메라 뷰
  Source/Aquarium/Tests/FishActorTests.cpp # Automation 테스트
  Scripts/import_bluetang.py               # FBX → 스켈레탈 메시 + 머티리얼
  Scripts/build_reef_m1.py                 # ReefM1 맵 생성
  Scripts/verify_scene.py                  # 맵 내용 검증
  Content/Fish/BlueTang/*, Content/Maps/ReefM1.umap, Content/Env/*  (스크립트 산출물)
```

---

### Task 0: 브랜치, LFS, 디렉터리

**Files:**
- Modify: `.gitattributes` (신규)
- Create: `assets/blender/export/.gitkeep`, `assets/textures/.gitkeep`, `unreal/Aquarium/Scripts/.gitkeep`

- [ ] **Step 1: 브랜치**

```bash
git checkout main && git pull && git checkout -b feat/m1-scene
```

- [ ] **Step 2: LFS 패턴 등록**

```bash
git lfs track "*.blend" "*.fbx" "*.png" "*.jpg" "*.exr" "*.uasset" "*.umap"
cat .gitattributes
```

Expected: 7개 패턴이 `filter=lfs diff=lfs merge=lfs -text`로 나열.

- [ ] **Step 3: 디렉터리**

```bash
mkdir -p assets/blender/export assets/textures unreal/Aquarium/Scripts
touch assets/blender/export/.gitkeep assets/textures/.gitkeep unreal/Aquarium/Scripts/.gitkeep
```

- [ ] **Step 4: 커밋**

```bash
git add .gitattributes assets unreal/Aquarium/Scripts
git commit -m "chore: LFS tracking and asset directories for M1"
```

---

### Task 1: SwimPlane — 2D 유영 평면을 3D 월드 좌표로 (규칙 계층)

**Files:**
- Create: `rules/include/aquarium/SwimPlane.h`
- Create: `tests/test_swimplane.cpp`
- Modify: `CMakeLists.txt` (`add_executable(rules_tests ...)`에 소스 추가)

규약: 평면은 원점 `origin`, 가로축 `right`, 세로축 `up`(모두 3D 단위 벡터)으로 정의. `ToWorld({x,y}) = origin + right*x + up*y`. `Forward(velocity2D)`는 속도의 3D 방향(정규화, 0이면 0).

- [ ] **Step 1: 실패하는 테스트**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/SwimPlane.h"

using namespace aquarium;
using Catch::Approx;

static SwimPlane XZPlane() {
    // Unreal: X forward, Y right, Z up. Plane faces the camera: right = +Y, up = +Z.
    return SwimPlane{Vec3{100.f, 0.f, 50.f}, Vec3{0.f, 1.f, 0.f}, Vec3{0.f, 0.f, 1.f}};
}

TEST_CASE("origin maps to plane origin") {
    Vec3 w = XZPlane().ToWorld({0.f, 0.f});
    REQUIRE(w.x == Approx(100.f)); REQUIRE(w.y == Approx(0.f)); REQUIRE(w.z == Approx(50.f));
}

TEST_CASE("x moves along right axis, y along up axis") {
    Vec3 w = XZPlane().ToWorld({3.f, -2.f});
    REQUIRE(w.x == Approx(100.f)); REQUIRE(w.y == Approx(3.f)); REQUIRE(w.z == Approx(48.f));
}

TEST_CASE("forward direction follows 2D velocity in world space") {
    Vec3 f = XZPlane().Forward({0.f, 5.f});
    REQUIRE(f.x == Approx(0.f)); REQUIRE(f.y == Approx(0.f)); REQUIRE(f.z == Approx(1.f));
}

TEST_CASE("forward of zero velocity is zero, not NaN") {
    Vec3 f = XZPlane().Forward({0.f, 0.f});
    REQUIRE(f.x == 0.f); REQUIRE(f.y == 0.f); REQUIRE(f.z == 0.f);
}
```

CMakeLists.txt `add_executable(rules_tests` 목록 끝에 `tests/test_swimplane.cpp` 추가.

- [ ] **Step 2: 실패 확인** — ctest 명령. Expected: `'aquarium/SwimPlane.h' file not found`.

- [ ] **Step 3: 구현** — `rules/include/aquarium/SwimPlane.h`

```cpp
#pragma once
#include <cmath>

#include "aquarium/Vec2.h"

namespace aquarium {

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 Normalized() const {
        const float len = Length();
        if (len <= 1e-6f) return {};
        return {x / len, y / len, z / len};
    }
};

// A 2D swim plane embedded in 3D: origin + right*x + up*y.
struct SwimPlane {
    Vec3 origin;
    Vec3 right{0.f, 1.f, 0.f};
    Vec3 up{0.f, 0.f, 1.f};

    Vec3 ToWorld(Vec2 p) const { return origin + right * p.x + up * p.y; }
    Vec3 Forward(Vec2 velocity) const { return (right * velocity.x + up * velocity.y).Normalized(); }
};

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — ctest 명령. Expected: `100% tests passed out of 46`.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/SwimPlane.h tests/test_swimplane.cpp CMakeLists.txt
git commit -m "feat: SwimPlane maps 2D swim space to 3D world"
```

---

### Task 2: SwimAnimation — 속도·시간 → 본 각도 (규칙 계층, F-13)

**Files:**
- Create: `rules/include/aquarium/SwimAnimation.h`
- Create: `tests/test_swimanimation.cpp`
- Modify: `CMakeLists.txt`

규칙: 척추 본 `N`개(머리→꼬리)에 대해 각도(도) = `amplitude(speed) * sin(2π * freq(speed) * t - phaseStep * i) + bend(turnRate)`. 진폭·주기는 속도에 비례하되 정지 시에도 작은 "숨쉬기" 진폭을 둔다. 꼬리로 갈수록 위상이 늦고 진폭이 커진다(계수 `1 + i*tailGain`). `bend`는 회전율에 비례한 측면 굽힘, 최대각으로 클램프.

- [ ] **Step 1: 실패하는 테스트**

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aquarium/SwimAnimation.h"

using namespace aquarium;
using Catch::Approx;

static SwimAnimParams P() {
    SwimAnimParams p;
    p.boneCount = 4;
    p.idleAmplitudeDeg = 2.f;
    p.amplitudePerSpeedDeg = 0.1f;   // deg per (unit/s)
    p.maxAmplitudeDeg = 15.f;
    p.idleFrequencyHz = 0.5f;
    p.frequencyPerSpeedHz = 0.01f;
    p.phaseStepRad = 0.8f;
    p.tailGain = 0.5f;
    p.bendPerTurnRateDeg = 0.2f;     // deg per (deg/s)
    p.maxBendDeg = 20.f;
    return p;
}

TEST_CASE("returns one angle per bone") {
    auto a = SwimAnimation::BoneAngles(0.f, 0.f, 0.f, P());
    REQUIRE(a.size() == 4);
}

TEST_CASE("stationary fish still breathes with idle amplitude") {
    // t chosen so sin(2π·0.5·t) = 1  ->  t = 0.5
    auto a = SwimAnimation::BoneAngles(0.f, 0.f, 0.5f, P());
    REQUIRE(a[0] == Approx(2.f));
}

TEST_CASE("amplitude grows with speed and is clamped") {
    auto slow = SwimAnimation::BoneAngles(50.f, 0.f, 0.f, P());   // freq = 1.0Hz -> sin(0)=0 at t=0
    auto fast = SwimAnimation::BoneAngles(1000.f, 0.f, 0.f, P());
    REQUIRE(SwimAnimation::Amplitude(50.f, P()) == Approx(7.f));      // 2 + 0.1*50
    REQUIRE(SwimAnimation::Amplitude(1000.f, P()) == Approx(15.f));   // clamped
    (void)slow; (void)fast;
}

TEST_CASE("tail bones lag in phase and swing wider") {
    const auto p = P();
    const float t = 0.25f;   // head: sin(2π·0.5·0.25) = sin(π/4)
    auto a = SwimAnimation::BoneAngles(0.f, 0.f, t, p);
    const float head = 2.f * std::sin(3.14159265f * 0.25f);
    const float tail = 2.f * (1.f + 3 * 0.5f) * std::sin(3.14159265f * 0.25f - 0.8f * 3);
    REQUIRE(a[0] == Approx(head).margin(1e-4f));
    REQUIRE(a[3] == Approx(tail).margin(1e-4f));
}

TEST_CASE("turning adds a clamped lateral bend to every bone") {
    const auto p = P();
    const auto base = SwimAnimation::BoneAngles(0.f, 0.f, 0.f, p);
    const auto turned = SwimAnimation::BoneAngles(0.f, 50.f, 0.f, p);      // bend = 0.2*50 = 10
    REQUIRE(turned[0] - base[0] == Approx(10.f));
    REQUIRE(turned[3] - base[3] == Approx(10.f));
    const auto clamped = SwimAnimation::BoneAngles(0.f, 500.f, 0.f, p);    // clamped to 20
    REQUIRE(clamped[0] - base[0] == Approx(20.f));
    const auto negative = SwimAnimation::BoneAngles(0.f, -500.f, 0.f, p);
    REQUIRE(negative[0] - base[0] == Approx(-20.f));
}
```

CMakeLists.txt에 `tests/test_swimanimation.cpp` 추가.

- [ ] **Step 2: 실패 확인** — ctest 명령. Expected: 헤더 없음 컴파일 실패.

- [ ] **Step 3: 구현** — `rules/include/aquarium/SwimAnimation.h`

```cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace aquarium {

struct SwimAnimParams {
    int boneCount = 6;
    float idleAmplitudeDeg = 2.f;
    float amplitudePerSpeedDeg = 0.1f;
    float maxAmplitudeDeg = 15.f;
    float idleFrequencyHz = 0.5f;
    float frequencyPerSpeedHz = 0.01f;
    float phaseStepRad = 0.8f;
    float tailGain = 0.5f;
    float bendPerTurnRateDeg = 0.2f;
    float maxBendDeg = 20.f;
};

// Procedural body wave (F-13): amplitude and frequency follow speed, bend follows turn rate.
struct SwimAnimation {
    static float Amplitude(float speed, const SwimAnimParams& p) {
        return std::min(p.idleAmplitudeDeg + p.amplitudePerSpeedDeg * speed, p.maxAmplitudeDeg);
    }
    static float Frequency(float speed, const SwimAnimParams& p) {
        return p.idleFrequencyHz + p.frequencyPerSpeedHz * speed;
    }
    static float Bend(float turnRateDegPerSec, const SwimAnimParams& p) {
        return std::clamp(p.bendPerTurnRateDeg * turnRateDegPerSec, -p.maxBendDeg, p.maxBendDeg);
    }

    // Angles in degrees for bones head(0) .. tail(boneCount-1).
    static std::vector<float> BoneAngles(float speed, float turnRateDegPerSec, float timeSec,
                                         const SwimAnimParams& p) {
        constexpr float kTwoPi = 6.283185307f;
        const float amp = Amplitude(speed, p);
        const float omega = kTwoPi * Frequency(speed, p);
        const float bend = Bend(turnRateDegPerSec, p);
        std::vector<float> out(static_cast<size_t>(std::max(p.boneCount, 0)));
        for (int i = 0; i < p.boneCount; ++i) {
            const float gain = 1.f + static_cast<float>(i) * p.tailGain;
            out[static_cast<size_t>(i)] =
                amp * gain * std::sin(omega * timeSec - p.phaseStepRad * static_cast<float>(i)) + bend;
        }
        return out;
    }
};

} // namespace aquarium
```

- [ ] **Step 4: 통과 확인** — ctest 명령. Expected: `100% tests passed out of 51`.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/SwimAnimation.h tests/test_swimanimation.cpp CMakeLists.txt
git commit -m "feat: procedural swim animation angles from speed and turn rate (F-13)"
```

---

### Task 3: Blender 배치 스크립트 — 블루탱 메시·리깅·재질·FBX

**Files:**
- Create: `assets/blender/make_bluetang.py`
- 산출물: `assets/blender/BlueTang.blend`, `assets/blender/export/BlueTang.fbx`, `assets/blender/export/T_BlueTang_BaseColor.png`

설계: 좌우 대칭 타원체를 lattice 없이 정점 스케일로 납작하게 만들고, Subdivision으로 부드럽게 한다. 지느러미는 평면 메시를 몸통에 붙인다. 아마추어는 X축(머리 +X, 꼬리 −X)을 따라 `Spine0..Spine5` + `Tail`, 가슴지느러미 `PecL/PecR`. 텍스처는 절차적 노드(파랑 몸통, 검은 팔레트 패턴, 노란 꼬리)를 2048² 이미지로 베이크한다.

- [ ] **Step 1: 스크립트 작성**

```python
# assets/blender/make_bluetang.py
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_bluetang.py
import bpy, bmesh, math, os
from mathutils import Vector

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")
os.makedirs(EXPORT, exist_ok=True)

# ---------- clean scene ----------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 0.01   # 1 unit = 1 cm, matches Unreal

BODY_LEN = 25.0   # cm
BODY_H = 12.0
BODY_W = 3.0

# ---------- body ----------
bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=1.0)
body = bpy.context.object
body.name = "BlueTang"
body.scale = (BODY_LEN / 2, BODY_W / 2, BODY_H / 2)
bpy.ops.object.transform_apply(scale=True)
# taper the tail: pull vertices with x<0 toward the axis
bm = bmesh.new(); bm.from_mesh(body.data)
for v in bm.verts:
    t = max(0.0, -v.co.x / (BODY_LEN / 2))     # 0 at center, 1 at tail tip
    v.co.z *= 1.0 - 0.55 * t * t
    v.co.y *= 1.0 - 0.4 * t
bm.to_mesh(body.data); bm.free()

# ---------- fins (flat quads joined to body) ----------
def add_fin(name, verts, faces):
    mesh = bpy.data.meshes.new(name); mesh.from_pydata(verts, [], faces); mesh.update()
    ob = bpy.data.objects.new(name, mesh); bpy.context.collection.objects.link(ob)
    return ob

L = BODY_LEN / 2
tail = add_fin("TailFin",
    [(-L + 1, 0, 2.5), (-L - 7, 0, 5.5), (-L - 6, 0, 0), (-L - 7, 0, -5.5), (-L + 1, 0, -2.5)],
    [(0, 1, 2), (0, 2, 4), (2, 3, 4)])
dorsal = add_fin("DorsalFin",
    [(L * 0.55, 0, BODY_H * 0.45), (0, 0, BODY_H * 0.5), (-L * 0.6, 0, BODY_H * 0.35),
     (-L * 0.6, 0, BODY_H * 0.62), (0, 0, BODY_H * 0.85), (L * 0.5, 0, BODY_H * 0.6)],
    [(0, 1, 4, 5), (1, 2, 3, 4)])
anal = add_fin("AnalFin",
    [(L * 0.1, 0, -BODY_H * 0.47), (-L * 0.6, 0, -BODY_H * 0.35),
     (-L * 0.6, 0, -BODY_H * 0.6), (L * 0.05, 0, -BODY_H * 0.75)],
    [(0, 1, 2, 3)])
pecL = add_fin("PecFinL",
    [(L * 0.35, BODY_W * 0.45, 0.5), (L * 0.1, BODY_W * 0.45 + 4.5, -1.5),
     (L * 0.0, BODY_W * 0.45 + 3.5, -2.5), (L * 0.25, BODY_W * 0.45, -1.5)],
    [(0, 1, 2, 3)])
pecR = add_fin("PecFinR",
    [(L * 0.35, -BODY_W * 0.45, 0.5), (L * 0.25, -BODY_W * 0.45, -1.5),
     (L * 0.0, -BODY_W * 0.45 - 3.5, -2.5), (L * 0.1, -BODY_W * 0.45 - 4.5, -1.5)],
    [(0, 1, 2, 3)])

fins = [tail, dorsal, anal, pecL, pecR]
for f in fins:
    sol = f.modifiers.new("Solidify", 'SOLIDIFY'); sol.thickness = 0.25; sol.offset = 0
bpy.ops.object.select_all(action='DESELECT')
for f in fins: f.select_set(True)
body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.convert(target='MESH')      # apply solidify on fins
bpy.ops.object.join()                      # everything into BlueTang
sub = body.modifiers.new("Subdivision", 'SUBSURF'); sub.levels = 1; sub.render_levels = 1
bpy.ops.object.shade_smooth()

# ---------- UV ----------
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
bpy.ops.object.mode_set(mode='OBJECT')

# ---------- procedural material -> bake ----------
mat = bpy.data.materials.new("M_BlueTang"); mat.use_nodes = True
nt = mat.node_tree; nt.nodes.clear()
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled"); bsdf.inputs["Roughness"].default_value = 0.35
geo = nt.nodes.new("ShaderNodeNewGeometry")
sep = nt.nodes.new("ShaderNodeSeparateXYZ")
nt.links.new(geo.outputs["Position"], sep.inputs[0])
# tail mask: x < -L*0.75  -> yellow
tail_ramp = nt.nodes.new("ShaderNodeMapRange")
tail_ramp.inputs["From Min"].default_value = -L * 0.72; tail_ramp.inputs["From Max"].default_value = -L * 0.80
nt.links.new(sep.outputs["X"], tail_ramp.inputs["Value"])
# palette pattern: dark band along the flank, shaped by |z| and x
noise = nt.nodes.new("ShaderNodeTexNoise"); noise.inputs["Scale"].default_value = 0.35
band = nt.nodes.new("ShaderNodeMath"); band.operation = 'ABSOLUTE'
nt.links.new(sep.outputs["Z"], band.inputs[0])
band_ramp = nt.nodes.new("ShaderNodeMapRange")
band_ramp.inputs["From Min"].default_value = BODY_H * 0.05; band_ramp.inputs["From Max"].default_value = BODY_H * 0.28
nt.links.new(band.outputs[0], band_ramp.inputs["Value"])
invert = nt.nodes.new("ShaderNodeMath"); invert.operation = 'SUBTRACT'; invert.inputs[0].default_value = 1.0
nt.links.new(band_ramp.outputs[0], invert.inputs[1])
mixblack = nt.nodes.new("ShaderNodeMix"); mixblack.data_type = 'RGBA'
mixblack.inputs["A"].default_value = (0.02, 0.18, 0.75, 1)   # royal blue
mixblack.inputs["B"].default_value = (0.01, 0.01, 0.02, 1)   # near black
nt.links.new(invert.outputs[0], mixblack.inputs["Factor"])
mixtail = nt.nodes.new("ShaderNodeMix"); mixtail.data_type = 'RGBA'
mixtail.inputs["B"].default_value = (0.95, 0.75, 0.05, 1)    # yellow
nt.links.new(mixblack.outputs["Result"], mixtail.inputs["A"])
nt.links.new(tail_ramp.outputs[0], mixtail.inputs["Factor"])
nt.links.new(mixtail.outputs["Result"], bsdf.inputs["Base Color"])
nt.links.new(bsdf.outputs[0], out.inputs[0])
body.data.materials.append(mat)

img = bpy.data.images.new("T_BlueTang_BaseColor", 2048, 2048)
tex_node = nt.nodes.new("ShaderNodeTexImage"); tex_node.image = img
nt.nodes.active = tex_node
scene.render.engine = 'CYCLES'; scene.cycles.samples = 16
scene.render.bake.use_pass_direct = False; scene.render.bake.use_pass_indirect = False
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.bake(type='DIFFUSE', margin=8)
img.filepath_raw = os.path.join(EXPORT, "T_BlueTang_BaseColor.png"); img.file_format = 'PNG'; img.save()
# rewire baked texture as the exported base color
nt.links.new(tex_node.outputs["Color"], bsdf.inputs["Base Color"])

# ---------- armature ----------
bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
arm = bpy.context.object; arm.name = "BlueTangRig"
ebones = arm.data.edit_bones
for b in list(ebones): ebones.remove(b)
root = ebones.new("Root"); root.head = (0, 0, 0); root.tail = (0, 0, 1)
SPINE = 6
x_head = L * 0.95; x_tail = -L * 0.9
step = (x_head - x_tail) / SPINE
prev = root
for i in range(SPINE):
    b = ebones.new(f"Spine{i}")
    b.head = (x_head - step * i, 0, 0); b.tail = (x_head - step * (i + 1), 0, 0)
    b.parent = prev; b.use_connect = (i > 0); prev = b
tailb = ebones.new("Tail"); tailb.head = prev.tail; tailb.tail = (-L - 6, 0, 0)
tailb.parent = prev; tailb.use_connect = True
for name, sign in (("PecL", 1), ("PecR", -1)):
    pb = ebones.new(name); pb.head = (L * 0.35, sign * BODY_W * 0.45, 0.5)
    pb.tail = (L * 0.05, sign * (BODY_W * 0.45 + 4.0), -2.0); pb.parent = ebones["Spine1"]
bpy.ops.object.mode_set(mode='OBJECT')

# ---------- skinning ----------
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type='ARMATURE_AUTO')

# ---------- save + export ----------
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "BlueTang.blend"))
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); arm.select_set(True)
bpy.ops.export_scene.fbx(filepath=os.path.join(EXPORT, "BlueTang.fbx"), use_selection=True,
                         apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
                         axis_forward='X', axis_up='Z', add_leaf_bones=False,
                         bake_anim=False, mesh_smooth_type='FACE', path_mode='COPY', embed_textures=False)
print("BLUETANG_OK verts=%d bones=%d" % (len(body.data.vertices), len(arm.data.bones)))
```

- [ ] **Step 2: 실행**

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_bluetang.py 2>&1 | grep -E "BLUETANG_OK|Error|Traceback" ; ls -la assets/blender/export/
```

Expected: `BLUETANG_OK verts=... bones=10` (Root + Spine0..5 + Tail + PecL + PecR), `BlueTang.fbx`와 `T_BlueTang_BaseColor.png`가 생성. Traceback이 나오면 해당 줄의 bpy API 이름을 Blender 5.2 Python 콘솔(`Blender -b --python-expr`)에서 확인해 고친다. 스크립트가 아니라 결과 파일을 수동 편집하지 않는다.

- [ ] **Step 3: 시각 확인** — Blender MCP 또는 Blender GUI로 `BlueTang.blend`를 열고 뷰포트 렌더 캡처를 한 장 남긴다:

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b assets/blender/BlueTang.blend --python-expr "import bpy; s=bpy.context.scene; s.render.engine='BLENDER_EEVEE'; s.render.resolution_x=1280; s.render.resolution_y=720; cam=bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); cam.location=(20,-45,10); cam.rotation_euler=(1.35,0,0.42); s.camera=cam; l=bpy.data.objects.new('Sun', bpy.data.lights.new('Sun','SUN')); s.collection.objects.link(l); l.rotation_euler=(0.8,0.3,0.5); s.render.filepath='$PWD/assets/blender/export/preview.png'; bpy.ops.render.render(write_still=True)"
```

Read 도구로 `assets/blender/export/preview.png`를 열어 형태(납작한 타원, 노란 꼬리, 검은 팔레트 무늬)를 확인한다. 형태가 어긋나면 스크립트의 수치를 고치고 Step 2부터 반복한다. 확인한 캡처는 커밋에 포함한다.

- [ ] **Step 4: ASSETS.md 기록** — `docs/ASSETS.md`의 "현재 도입한 외부 에셋은 없다" 아래에 추가:

```markdown
## 직접 제작

| 에셋 | 제작 도구 | 생성 스크립트 | 산출물 | 외부 텍스처 |
|---|---|---|---|---|
| 블루탱 (BlueTang) | Blender 5.2.2 LTS | `assets/blender/make_bluetang.py` | `assets/blender/BlueTang.blend`, `assets/blender/export/BlueTang.fbx`, `T_BlueTang_BaseColor.png` (2048², 절차적 노드 베이크) | 없음 |
```

- [ ] **Step 5: 커밋**

```bash
git add assets/blender docs/ASSETS.md
git commit -m "feat: scripted Blender blue tang mesh, rig, baked texture, FBX export"
```

---

### Task 4: Unreal 모듈이 규칙 계층을 컴파일한다

**Files:**
- Modify: `unreal/Aquarium/Source/Aquarium/Aquarium.Build.cs`
- Create: `unreal/Aquarium/Source/Aquarium/Tests/RulesLinkTests.cpp`

- [ ] **Step 1: 실패하는 테스트** — `Tests/RulesLinkTests.cpp`

```cpp
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "aquarium/Nickname.h"
#include "aquarium/Steering.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAquariumRulesLinkTest, "Aquarium.Rules.LinksIntoModule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAquariumRulesLinkTest::RunTest(const FString& Parameters)
{
    const aquarium::NicknameResult r = aquarium::ValidateNickname("  nemo ");
    TestTrue(TEXT("nickname valid"), r.ok);
    TestEqual(TEXT("trimmed"), FString(r.value.c_str()), FString(TEXT("nemo")));
    const aquarium::Vec2 v = aquarium::SteeringVector({true, false, false, true});
    TestTrue(TEXT("diagonal normalized"), FMath::IsNearlyEqual(v.Length(), 1.f, 1e-4f));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: 실패 확인** — UE 빌드. Expected: `'aquarium/Nickname.h' file not found`.

- [ ] **Step 3: Build.cs 수정** — 전체 교체

```csharp
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class Aquarium : ModuleRules
{
	public Aquarium(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp17;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		// Engine-independent rules layer lives at the repo root; compile the same sources here (no copies).
		string RepoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
		string RulesDir = Path.Combine(RepoRoot, "rules");
		PublicIncludePaths.Add(Path.Combine(RulesDir, "include"));
		ConditionalAddModuleDirectory(new DirectoryReference(Path.Combine(RulesDir, "src")));
		bEnableExceptions = false;

		// Automation tests use FAutomationEditorCommonUtils (editor only).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
```

`ConditionalAddModuleDirectory`는 UBT가 해당 디렉터리의 `.cpp`를 이 모듈 소스로 포함하게 한다.

- [ ] **Step 4: 통과 확인** — UE 빌드 → `Result: Succeeded`. 이어서 UE 테스트. Expected 출력에 `Aquarium.Rules.LinksIntoModule ... Success`. 실패 시 로그 전체를 `grep -B5 -A5 LinksIntoModule`로 확인.

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Source
git commit -m "feat: Unreal module compiles the shared rules layer, link test"
```

---

### Task 5: FBX 임포트 스크립트 → 스켈레탈 메시와 머티리얼

**Files:**
- Create: `unreal/Aquarium/Scripts/import_bluetang.py`
- 산출물: `unreal/Aquarium/Content/Fish/BlueTang/{SK_BlueTang, SK_BlueTang_Skeleton, SK_BlueTang_PhysicsAsset, T_BlueTang_BaseColor, M_BlueTang}.uasset`

- [ ] **Step 1: 스크립트**

```python
# unreal/Aquarium/Scripts/import_bluetang.py
import os, unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
FBX = os.path.join(ROOT, "assets", "blender", "export", "BlueTang.fbx")
PNG = os.path.join(ROOT, "assets", "blender", "export", "T_BlueTang_BaseColor.png")
DEST = "/Game/Fish/BlueTang"

def import_asset(filename, options=None):
    task = unreal.AssetImportTask()
    task.filename = filename; task.destination_path = DEST
    task.automated = True; task.replace_existing = True; task.save = True
    if options: task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return list(task.imported_object_paths)

# texture
tex_paths = import_asset(PNG)
tex = unreal.load_asset(tex_paths[0]); tex.set_editor_property("srgb", True)

# skeletal mesh
opt = unreal.FbxImportUI()
opt.import_mesh = True; opt.import_as_skeletal = True; opt.import_animations = False
opt.import_materials = False; opt.import_textures = False; opt.create_physics_asset = True
opt.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
opt.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
opt.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
sk_paths = import_asset(FBX, opt)
sk = next(unreal.load_asset(p) for p in sk_paths if unreal.load_asset(p).__class__.__name__ == "SkeletalMesh")

# material: texture -> base color
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_BlueTang", DEST, unreal.Material, unreal.MaterialFactoryNew())
mel = unreal.MaterialEditingLibrary
node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
node.texture = tex
mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
rough.r = 0.35
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)

mats = sk.get_editor_property("materials")
mats[0].material_interface = mat
sk.set_editor_property("materials", mats)
unreal.EditorAssetLibrary.save_loaded_asset(sk)

bounds = sk.get_bounds().box_extent
print("IMPORT_OK mesh=%s extent=%s" % (sk.get_path_name(), bounds))
```

- [ ] **Step 2: 실행** — UE 파이썬 `import_bluetang.py`. Expected: `IMPORT_OK mesh=/Game/Fish/BlueTang/SK_BlueTang...` 와 `extent`의 X≈12.5·Z≈6 (cm, 몸길이 25cm). 본 이름(`Spine0`, `Spine5`, `Tail`)의 존재는 Task 6의 Automation 테스트 `Aquarium.Fish.SpineBonesExistOnMesh`가 판정한다.

- [ ] **Step 3: 커밋**

```bash
git add unreal/Aquarium/Scripts/import_bluetang.py unreal/Aquarium/Content/Fish
git commit -m "feat: scripted FBX import of blue tang skeletal mesh and material"
```

---

### Task 6: AFishActor — 규칙 계층으로 움직이고 본을 흔든다

**Files:**
- Create: `unreal/Aquarium/Source/Aquarium/FishActor.h`, `FishActor.cpp`
- Create: `unreal/Aquarium/Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 1: 실패하는 테스트** — `Tests/FishActorTests.cpp`

```cpp
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "FishActor.h"

namespace {
AFishActor* SpawnFish(UWorld* World, uint32 Seed)
{
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFishActor* Fish = World->SpawnActor<AFishActor>(AFishActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    Fish->Seed = Seed;
    Fish->PlaneOrigin = FVector(300.f, 0.f, 100.f);
    Fish->PlaneHalfWidth = 300.f; Fish->PlaneHalfHeight = 150.f;
    Fish->InitializeSwim();
    return Fish;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorMovesByRules, "Aquarium.Fish.MovesByRulesLayer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorMovesByRules::RunTest(const FString&)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    AFishActor* Fish = SpawnFish(World, 7u);
    const FVector Before = Fish->GetActorLocation();
    for (int i = 0; i < 30; ++i) Fish->StepSwim(0.1f);
    const FVector After = Fish->GetActorLocation();
    TestTrue(TEXT("fish moved"), !After.Equals(Before, 1.f));
    TestTrue(TEXT("stays on plane X"), FMath::IsNearlyEqual(After.X, 300.f, 1e-2f));
    TestTrue(TEXT("inside plane Y"), FMath::Abs(After.Y) <= 300.f + 1e-2f);
    TestTrue(TEXT("inside plane Z"), FMath::Abs(After.Z - 100.f) <= 150.f + 1e-2f);
    TestTrue(TEXT("speed reported"), Fish->CurrentSpeed() > 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorZeroDt, "Aquarium.Fish.ZeroDtDoesNotMove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorZeroDt::RunTest(const FString&)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    AFishActor* Fish = SpawnFish(World, 7u);
    Fish->StepSwim(0.1f);
    const FVector Before = Fish->GetActorLocation();
    Fish->StepSwim(0.f);
    TestTrue(TEXT("no move at dt=0"), Fish->GetActorLocation().Equals(Before, 1e-3f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorDeterministic, "Aquarium.Fish.SameSeedSamePath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorDeterministic::RunTest(const FString&)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    AFishActor* A = SpawnFish(World, 42u); AFishActor* B = SpawnFish(World, 42u);
    for (int i = 0; i < 50; ++i) { A->StepSwim(0.05f); B->StepSwim(0.05f); }
    TestTrue(TEXT("same path"), A->GetActorLocation().Equals(B->GetActorLocation(), 1e-3f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorSpineBonesExist, "Aquarium.Fish.SpineBonesExistOnMesh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorSpineBonesExist::RunTest(const FString&)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    AFishActor* Fish = SpawnFish(World, 1u);
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
    if (!TestNotNull(TEXT("SK_BlueTang loads"), Mesh)) return false;
    Fish->SetMesh(Mesh);
    for (const TCHAR* Name : {TEXT("Spine0"), TEXT("Spine5"), TEXT("Tail")})
        TestTrue(FString::Printf(TEXT("bone %s exists"), Name), Fish->HasBone(FName(Name)));
    Fish->StepSwim(0.1f);
    TestTrue(TEXT("tail bone rotated by swim wave"),
             !Fish->BoneRotation(FName(TEXT("Tail"))).IsNearlyZero(1e-3f));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: 실패 확인** — UE 빌드. Expected: `'FishActor.h' file not found`.

- [ ] **Step 3: 구현** — `FishActor.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"

#include "aquarium/Bounds.h"
#include "aquarium/Motion.h"
#include "aquarium/SwimAnimation.h"
#include "aquarium/SwimPlane.h"
#include "aquarium/Wander.h"

#include "FishActor.generated.h"

// A fish driven entirely by the engine-independent rules layer. The actor only maps
// 2D swim-plane state to world transforms and bone rotations.
UCLASS()
class AQUARIUM_API AFishActor : public AActor
{
    GENERATED_BODY()

public:
    AFishActor();

    UPROPERTY(EditAnywhere, Category = "Swim") uint32 Seed = 1;
    UPROPERTY(EditAnywhere, Category = "Swim") FVector PlaneOrigin = FVector(600.f, 0.f, 120.f);
    UPROPERTY(EditAnywhere, Category = "Swim") float PlaneHalfWidth = 300.f;   // cm
    UPROPERTY(EditAnywhere, Category = "Swim") float PlaneHalfHeight = 150.f;  // cm
    UPROPERTY(EditAnywhere, Category = "Swim") float AvoidDistance = 50.f;
    UPROPERTY(EditAnywhere, Category = "Swim") float MaxSpeed = 40.f;          // cm/s
    UPROPERTY(EditAnywhere, Category = "Swim") float Accel = 30.f;
    UPROPERTY(EditAnywhere, Category = "Swim") float Decel = 40.f;
    UPROPERTY(EditAnywhere, Category = "Swim") USkeletalMesh* FishMesh = nullptr;

    void InitializeSwim();
    void StepSwim(float DeltaSeconds);
    float CurrentSpeed() const { return Motion.velocity.Length(); }

    void SetMesh(USkeletalMesh* Mesh);
    bool HasBone(FName Bone) const;
    FRotator BoneRotation(FName Bone) const;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere) UPoseableMeshComponent* Body = nullptr;

    aquarium::SwimPlane Plane;
    aquarium::Rect Area;
    aquarium::MotionState Motion;
    aquarium::MotionParams MotionParamsValue;
    aquarium::SwimAnimParams AnimParams;
    TOptional<aquarium::WanderBehavior> Wander;
    float SwimTime = 0.f;
    float LastYaw = 0.f;

    static const TArray<FName>& SpineBoneNames();
};
```

`FishActor.cpp`

```cpp
#include "FishActor.h"

AFishActor::AFishActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
    RootComponent = Body;
}

const TArray<FName>& AFishActor::SpineBoneNames()
{
    static const TArray<FName> Names = {TEXT("Spine0"), TEXT("Spine1"), TEXT("Spine2"),
                                        TEXT("Spine3"), TEXT("Spine4"), TEXT("Spine5"), TEXT("Tail")};
    return Names;
}

void AFishActor::InitializeSwim()
{
    Plane.origin = {static_cast<float>(PlaneOrigin.X), static_cast<float>(PlaneOrigin.Y), static_cast<float>(PlaneOrigin.Z)};
    Plane.right = {0.f, 1.f, 0.f};
    Plane.up = {0.f, 0.f, 1.f};
    Area = {-PlaneHalfWidth, -PlaneHalfHeight, PlaneHalfWidth, PlaneHalfHeight};
    Motion = {};
    MotionParamsValue.maxSpeed = MaxSpeed; MotionParamsValue.accel = Accel; MotionParamsValue.decel = Decel;
    AnimParams.boneCount = SpineBoneNames().Num();
    Wander.Emplace(Seed, Area, /*arriveRadius*/ 15.f, /*targetLifetime*/ 8.f);
    SwimTime = 0.f; LastYaw = 0.f;
    if (FishMesh) SetMesh(FishMesh);
    const aquarium::Vec3 w = Plane.ToWorld(Motion.position);
    SetActorLocation(FVector(w.x, w.y, w.z));
}

void AFishActor::StepSwim(float DeltaSeconds)
{
    if (!Wander.IsSet() || DeltaSeconds <= 0.f) return;

    Wander->Update(Motion.position, DeltaSeconds);
    aquarium::Vec2 dir = aquarium::AvoidBoundary(Motion.position, Wander->DesiredDirection(Motion.position), Area, AvoidDistance);
    aquarium::StepMotion(Motion, dir, MotionParamsValue, DeltaSeconds);
    Motion.position = aquarium::ClampToArea(Motion.position, Area);

    const aquarium::Vec3 w = Plane.ToWorld(Motion.position);
    SetActorLocation(FVector(w.x, w.y, w.z));

    const aquarium::Vec3 f = Plane.Forward(Motion.velocity);
    float TurnRate = 0.f;
    if (f.Length() > 0.f)
    {
        const FRotator Look = FVector(f.x, f.y, f.z).Rotation();
        TurnRate = FMath::FindDeltaAngleDegrees(LastYaw, Look.Yaw) / DeltaSeconds;
        LastYaw = Look.Yaw;
        SetActorRotation(Look);
    }

    SwimTime += DeltaSeconds;
    const std::vector<float> Angles = aquarium::SwimAnimation::BoneAngles(CurrentSpeed(), TurnRate, SwimTime, AnimParams);
    const TArray<FName>& Bones = SpineBoneNames();
    for (int32 i = 0; i < Bones.Num() && i < static_cast<int32>(Angles.size()); ++i)
    {
        if (Body->GetBoneIndex(Bones[i]) == INDEX_NONE) continue;
        Body->SetBoneRotationByName(Bones[i], FRotator(0.f, Angles[static_cast<size_t>(i)], 0.f), EBoneSpaces::ComponentSpace);
    }
}

void AFishActor::SetMesh(USkeletalMesh* Mesh)
{
    FishMesh = Mesh;
    Body->SetSkinnedAssetAndUpdate(Mesh);
}

bool AFishActor::HasBone(FName Bone) const { return Body->GetBoneIndex(Bone) != INDEX_NONE; }

FRotator AFishActor::BoneRotation(FName Bone) const
{
    return Body->GetBoneRotationByName(Bone, EBoneSpaces::ComponentSpace);
}

void AFishActor::BeginPlay()
{
    Super::BeginPlay();
    InitializeSwim();
}

void AFishActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    StepSwim(DeltaSeconds);
}
```

주의: `Wander`가 `TOptional`인 이유는 `WanderBehavior`에 기본 생성자가 없기 때문이다. `<vector>`·`<optional>` 등 표준 헤더는 규칙 헤더가 이미 포함한다.

- [ ] **Step 4: 통과 확인** — UE 빌드 `Result: Succeeded` → UE 테스트. Expected: `Aquarium.Fish.*` 4개 `Success`. `SpineBonesExistOnMesh`가 본 이름으로 실패하면 FBX 임포트 결과의 본 이름(예: `BlueTangRig:Spine0`)을 확인해 Task 3의 export 옵션(`use_armature_deform_only`, 네임스페이스) 또는 `SpineBoneNames()`를 맞춘다.

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Source
git commit -m "feat: AFishActor drives movement and body wave from the rules layer"
```

---

### Task 7: 게임 모드와 고정 잠수부 카메라

**Files:**
- Create: `unreal/Aquarium/Source/Aquarium/AquariumGameMode.h`, `.cpp`
- Create: `unreal/Aquarium/Source/Aquarium/DiverPlayerController.h`, `.cpp`
- Modify: `unreal/Aquarium/Config/DefaultEngine.ini`

- [ ] **Step 1: 코드**

`DiverPlayerController.h`

```cpp
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DiverPlayerController.generated.h"

// Looks through the actor tagged "DiverCamera"; never follows the fish (P-06).
UCLASS()
class AQUARIUM_API ADiverPlayerController : public APlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
};
```

`DiverPlayerController.cpp`

```cpp
#include "DiverPlayerController.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"

void ADiverPlayerController::BeginPlay()
{
    Super::BeginPlay();
    TArray<AActor*> Cameras;
    UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ACameraActor::StaticClass(), FName(TEXT("DiverCamera")), Cameras);
    if (Cameras.Num() > 0) SetViewTarget(Cameras[0]);
    bShowMouseCursor = false;
}
```

`AquariumGameMode.h`

```cpp
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AquariumGameMode.generated.h"

UCLASS()
class AQUARIUM_API AAquariumGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAquariumGameMode();
};
```

`AquariumGameMode.cpp`

```cpp
#include "AquariumGameMode.h"
#include "DiverPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

AAquariumGameMode::AAquariumGameMode()
{
    PlayerControllerClass = ADiverPlayerController::StaticClass();
    DefaultPawnClass = ASpectatorPawn::StaticClass();   // no visible pawn; view comes from DiverCamera
}
```

`DefaultEngine.ini` — `[/Script/EngineSettings.GameMapsSettings]` 블록을 다음으로 교체:

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/Maps/ReefM1
EditorStartupMap=/Game/Maps/ReefM1
GlobalDefaultGameMode=/Script/Aquarium.AquariumGameMode
```

- [ ] **Step 2: 빌드 확인** — UE 빌드. Expected: `Result: Succeeded`.

- [ ] **Step 3: 커밋**

```bash
git add unreal/Aquarium/Source unreal/Aquarium/Config/DefaultEngine.ini
git commit -m "feat: game mode with fixed diver camera view (P-06)"
```

---

### Task 8: 모래 텍스처 (Poly Haven CC0)

**Files:**
- Create: `assets/textures/sand/` (diffuse·normal·roughness 2K)
- Modify: `docs/ASSETS.md`

- [ ] **Step 1: 후보 조회와 다운로드** — Poly Haven 공개 API로 모래 텍스처 중 첫 항목을 고른다:

```bash
curl -s "https://api.polyhaven.com/assets?t=textures&c=sand" | python3 -c "import sys,json; d=json.load(sys.stdin); print('\n'.join(f'{k}: {v[\"name\"]}' for k,v in list(d.items())[:8]))"
```

출력에서 `sand`가 이름에 들어간 첫 id(이하 `<id>`)를 고르고:

```bash
curl -s "https://api.polyhaven.com/files/<id>" | python3 -c "
import sys,json; f=json.load(sys.stdin)
for k in ('Diffuse','nor_gl','Rough'):
    print(k, f[k]['2k']['png']['url'])"
```

세 URL을 `assets/textures/sand/<id>_{diff,nor_gl,rough}_2k.png`로 `curl -L -o` 저장.

- [ ] **Step 2: ASSETS.md 기록** — "## 조사 후보" 위에 추가:

```markdown
## 도입한 외부 에셋

| 에셋 | 저작자 | 원본 URL | 다운로드일 | 라이선스 | 출처 표기 | 수정 | 게임 배포 | 공개 저장소 재배포 | 로컬 경로 |
|---|---|---|---|---|---|---|---|---|---|
| Poly Haven `<id>` (모래 텍스처 2K) | Poly Haven 기여자 (사이트 표기) | https://polyhaven.com/a/<id> | <날짜> | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Textures from Poly Haven") | 없음 | 가능 | 가능 | `assets/textures/sand/` |
```

`<id>`와 날짜를 실제 값으로 채운다.

- [ ] **Step 3: 커밋**

```bash
git add assets/textures docs/ASSETS.md
git commit -m "chore: add Poly Haven CC0 sand texture with provenance"
```

---

### Task 9: ReefM1 맵 생성 스크립트

**Files:**
- Create: `unreal/Aquarium/Scripts/build_reef_m1.py`
- Create: `unreal/Aquarium/Scripts/verify_scene.py`
- 산출물: `Content/Maps/ReefM1.umap`, `Content/Env/{T_Sand_*, M_Sand, M_Caustics}`

- [ ] **Step 1: 검증 스크립트 먼저** — `verify_scene.py` (실패해야 정상인 상태에서 시작)

```python
# unreal/Aquarium/Scripts/verify_scene.py
import unreal
ok = unreal.EditorLevelLibrary.load_level("/Game/Maps/ReefM1")
assert ok, "ReefM1 does not load"
actors = unreal.EditorLevelLibrary.get_all_level_actors()
by_class = {}
for a in actors: by_class.setdefault(a.get_class().get_name(), []).append(a)
need = {"CameraActor": 1, "DirectionalLight": 1, "SkyLight": 1, "ExponentialHeightFog": 1, "StaticMeshActor": 1, "FishActor": 1}
missing = [k for k, n in need.items() if len(by_class.get(k, [])) < n]
assert not missing, "missing actors: %s" % missing
cam = by_class["CameraActor"][0]
assert "DiverCamera" in [str(t) for t in cam.tags], "camera lacks DiverCamera tag"
fog = by_class["ExponentialHeightFog"][0].get_editor_property("component")
assert fog.get_editor_property("fog_density") >= 0.02, "fog too thin for underwater look"
print("SCENE_OK actors=%d" % len(actors))
```

- [ ] **Step 2: 실패 확인** — UE 파이썬 `verify_scene.py`. Expected: `AssertionError: ReefM1 does not load`.

- [ ] **Step 3: 맵 생성 스크립트** — `build_reef_m1.py`

```python
# unreal/Aquarium/Scripts/build_reef_m1.py
import os, glob, unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
SAND_DIR = os.path.join(ROOT, "assets", "textures", "sand")
ENV = "/Game/Env"
MAP = "/Game/Maps/ReefM1"
ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary

# ---- textures ----
def import_tex(path, name):
    task = unreal.AssetImportTask(); task.filename = path; task.destination_path = ENV
    task.destination_name = name; task.automated = True; task.replace_existing = True; task.save = True
    tools.import_asset_tasks([task]); return unreal.load_asset(task.imported_object_paths[0])

diff = import_tex(glob.glob(os.path.join(SAND_DIR, "*_diff_2k.png"))[0], "T_Sand_D")
nor = import_tex(glob.glob(os.path.join(SAND_DIR, "*_nor_gl_2k.png"))[0], "T_Sand_N")
rough = import_tex(glob.glob(os.path.join(SAND_DIR, "*_rough_2k.png"))[0], "T_Sand_R")
nor.set_editor_property("srgb", False); nor.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
rough.set_editor_property("srgb", False)
for t in (diff, nor, rough): eal.save_loaded_asset(t)

# ---- sand material ----
m_sand = tools.create_asset("M_Sand", ENV, unreal.Material, unreal.MaterialFactoryNew())
coord = mel.create_material_expression(m_sand, unreal.MaterialExpressionTextureCoordinate, -900, 0)
coord.u_tiling = 20.0; coord.v_tiling = 20.0
def sample(tex, y, is_normal=False):
    n = mel.create_material_expression(m_sand, unreal.MaterialExpressionTextureSample, -600, y)
    n.texture = tex
    if is_normal: n.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    mel.connect_material_expressions(coord, "", n, "UVs"); return n
mel.connect_material_property(sample(diff, -200), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(sample(nor, 100, True), "RGB", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(sample(rough, 400), "R", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(m_sand); eal.save_loaded_asset(m_sand)

# ---- caustics light function (procedural, no texture) ----
m_caus = tools.create_asset("M_Caustics", ENV, unreal.Material, unreal.MaterialFactoryNew())
m_caus.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
wc = mel.create_material_expression(m_caus, unreal.MaterialExpressionWorldPosition, -1200, 0)
scale = mel.create_material_expression(m_caus, unreal.MaterialExpressionMultiply, -1000, 0)
sc = mel.create_material_expression(m_caus, unreal.MaterialExpressionConstant, -1200, 200); sc.r = 0.01
mel.connect_material_expressions(wc, "", scale, "A"); mel.connect_material_expressions(sc, "", scale, "B")
pan = mel.create_material_expression(m_caus, unreal.MaterialExpressionPanner, -800, 0)
pan.speed_x = 0.05; pan.speed_y = 0.03
mel.connect_material_expressions(scale, "", pan, "Coordinate")
noise = mel.create_material_expression(m_caus, unreal.MaterialExpressionNoise, -600, 0)
noise.scale = 4.0; noise.levels = 3; noise.output_min = 0.0; noise.output_max = 1.0
mel.connect_material_expressions(pan, "", noise, "Position")
pw = mel.create_material_expression(m_caus, unreal.MaterialExpressionPower, -350, 0)
pe = mel.create_material_expression(m_caus, unreal.MaterialExpressionConstant, -500, 200); pe.r = 3.0
mel.connect_material_expressions(noise, "", pw, "Base"); mel.connect_material_expressions(pe, "", pw, "Exp")
lift = mel.create_material_expression(m_caus, unreal.MaterialExpressionAdd, -150, 0)
lc = mel.create_material_expression(m_caus, unreal.MaterialExpressionConstant, -350, 200); lc.r = 0.35
mel.connect_material_expressions(pw, "", lift, "A"); mel.connect_material_expressions(lc, "", lift, "B")
mel.connect_material_property(lift, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m_caus); eal.save_loaded_asset(m_caus)

# ---- level ----
world = ell.new_level(MAP)
def spawn(cls, loc, rot=unreal.Rotator(0, 0, 0)):
    return ell.spawn_actor_from_class(cls, unreal.Vector(*loc), rot)

# sand floor: 40m x 40m plane at z=0
floor = spawn(unreal.StaticMeshActor, (0, 0, 0))
floor.set_actor_label("SandFloor")
smc = floor.static_mesh_component
smc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
smc.set_material(0, m_sand)
floor.set_actor_scale3d(unreal.Vector(40, 40, 1))
smc.set_mobility(unreal.ComponentMobility.STATIC)

# sun from above the surface, steep angle, warm; volumetric scattering for light shafts
sun = spawn(unreal.DirectionalLight, (0, 0, 1000), unreal.Rotator(-65, 30, 0))
sun.set_actor_label("Sun")
sc = sun.light_component
sc.set_intensity(8.0); sc.set_light_color(unreal.LinearColor(1.0, 0.95, 0.85))
sc.set_editor_property("volumetric_scattering_intensity", 4.0)
sc.set_editor_property("light_function_material", m_caus)
sc.set_editor_property("light_function_scale", unreal.Vector(400, 400, 400))

sky = spawn(unreal.SkyLight, (0, 0, 500)); sky.set_actor_label("Sky")
sky.light_component.set_intensity(0.6); sky.light_component.set_light_color(unreal.LinearColor(0.35, 0.65, 0.9))

fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0)); fog.set_actor_label("Water")
fc = fog.component
fc.set_editor_property("fog_density", 0.05)
fc.set_editor_property("fog_height_falloff", 0.05)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.02, 0.18, 0.30))
fc.set_editor_property("volumetric_fog", True)
fc.set_editor_property("volumetric_fog_scattering_distribution", 0.6)
fc.set_editor_property("volumetric_fog_extinction_scale", 2.0)
fc.set_editor_property("start_distance", 0.0)

cam = spawn(unreal.CameraActor, (0, 0, 130), unreal.Rotator(-3, 0, 0))
cam.set_actor_label("DiverCamera"); cam.tags = [unreal.Name("DiverCamera")]
cam.camera_component.set_field_of_view(75.0)

fish_cls = unreal.load_class(None, "/Script/Aquarium.FishActor")
fish = spawn(fish_cls, (600, 0, 120)); fish.set_actor_label("BlueTang")
fish.set_editor_property("FishMesh", unreal.load_asset("/Game/Fish/BlueTang/SK_BlueTang"))
fish.set_editor_property("PlaneOrigin", unreal.Vector(600, 0, 120))
fish.set_editor_property("Seed", 7)

ell.save_current_level()
print("REEF_OK")
```

- [ ] **Step 4: 실행과 검증**

```bash
# 생성
UE 파이썬 build_reef_m1.py    → Expected: REEF_OK
# 검증
UE 파이썬 verify_scene.py     → Expected: SCENE_OK actors=6 이상
```

`set_editor_property` 이름이 다르면(`LogPython: Error: ... has no property`) 해당 클래스의 프로퍼티 이름을 `unreal.log(dir(obj))`로 확인해 스크립트를 고친다. 산출물(.umap/.uasset)은 손으로 편집하지 않는다.

- [ ] **Step 5: 시각 캡처** — 에디터 GUI를 MCP와 함께 띄우고(`SETUP.md`의 명령) `CaptureViewport`로 한 장 캡처해 Read 도구로 확인한다. 기대: 푸른 안개 속 모래 바닥, 위에서 내려오는 빛줄기, 바닥의 밝은 무늬, 가운데 파란 물고기. 안 보이는 요소가 있으면 `build_reef_m1.py`의 수치(안개 밀도·밝기·카메라 위치)를 고치고 Step 4부터 반복. 캡처 파일은 `docs/reviews/<날짜>-m1-still.png`로 저장(LFS).

- [ ] **Step 6: 커밋**

```bash
git add unreal/Aquarium/Scripts unreal/Aquarium/Content/Maps unreal/Aquarium/Content/Env docs/reviews
git commit -m "feat: scripted ReefM1 underwater scene with sand, fog, light shafts, caustics"
```

---

### Task 10: 30초 유영 영상

**Files:**
- Create: `scripts/render_m1_video.sh`
- 산출물: `docs/reviews/<날짜>-m1-swim.mp4` (LFS)

- [ ] **Step 1: 스크립트**

```bash
#!/bin/bash
# Renders a deterministic 30fps, 35s run of ReefM1 to PNG frames, then encodes with ffmpeg.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor"
OUT="$ROOT/unreal/Aquarium/Saved/VideoFrames"
rm -rf "$OUT"; mkdir -p "$OUT" "$ROOT/docs/reviews"
"$UE" "$ROOT/unreal/Aquarium/Aquarium.uproject" ReefM1 -game -windowed -ResX=1920 -ResY=1080 \
  -benchmark -fps=30 -seconds=35 -dumpmovie -notexturestreaming -unattended -nosplash \
  -log 2>&1 | grep -E "LogInit: Engine Version|Fatal|DumpMovie" | head -5
# -dumpmovie writes to Saved/Screenshots/MacEditor or Saved/Screenshots/Mac
FRAMES=$(ls -d "$ROOT"/unreal/Aquarium/Saved/Screenshots/*/ | head -1)
ls "$FRAMES" | head -3
DATE=$(date +%F)
ffmpeg -y -framerate 30 -pattern_type glob -i "$FRAMES/*.png" -c:v libx264 -pix_fmt yuv420p -crf 18 \
  "$ROOT/docs/reviews/$DATE-m1-swim.mp4"
echo "VIDEO_OK $ROOT/docs/reviews/$DATE-m1-swim.mp4"
```

- [ ] **Step 2: 실행**

```bash
chmod +x scripts/render_m1_video.sh && ./scripts/render_m1_video.sh
```

Expected: `VIDEO_OK ...mp4`, 프레임 수 ≈ 1050. `-dumpmovie`가 PNG 대신 BMP를 쓰면 ffmpeg 입력 패턴을 `*.bmp`로 바꾼다. 프레임이 하나도 없으면 `-game` 실행 로그에서 맵 로드 실패(`ReefM1`)나 게임 모드 오류를 확인한다.

- [ ] **Step 3: 영상 자체 확인** — ffmpeg로 5초·15초·30초 지점 스틸 3장을 뽑아 Read 도구로 본다:

```bash
for t in 5 15 30; do ffmpeg -y -ss $t -i docs/reviews/*-m1-swim.mp4 -frames:v 1 /tmp/claude-501/m1_$t.png; done
```

확인 항목(SRS 시각 기준): 물고기가 순간적으로 좌우 반전하지 않는가, 회전 시 몸이 굽는가, 경계에서 튀지 않는가. 문제가 보이면 `SwimAnimParams`·`MotionParams` 기본값이나 `AvoidDistance`를 조정하고 규칙 테스트를 유지한 채 Step 2를 반복한다.

- [ ] **Step 4: 사용자에게 전달** — `SendUserFile`로 mp4와 스틸을 보내고 시각 검토를 요청한다. 검토 결과(승인/수정 요청과 코멘트)를 받을 때까지 M1을 완료로 표시하지 않는다.

- [ ] **Step 5: 커밋**

```bash
git add scripts/render_m1_video.sh docs/reviews
git commit -m "feat: deterministic 30s swim video render script and first M1 review video"
```

---

### Task 11: 문서 갱신과 마무리

**Files:**
- Modify: `docs/TASK.md`, `docs/SETUP.md`, `README.md`

- [ ] **Step 1: 전체 검증 재실행** — ctest 명령(51개 통과), UE 빌드(Succeeded), UE 테스트(`Aquarium.*` 5개 Success). 결과 문자열을 그대로 기록에 쓴다.

- [ ] **Step 2: TASK.md** — M1 행에 완료 조건별 결과를 적는다: 규칙 테스트 수·커밋, Unreal 테스트 5개 결과, 영상 경로, 사용자 검토 결과(날짜·코멘트). "검증 현황"에 "시각 검토: 사용자 승인/보류(코멘트)"를 추가한다. README "현재 상태"를 "규칙 계층과 M1 수중 장면·블루탱 유영 영상까지 완료, 시각 검토 결과: …"로 갱신한다.

- [ ] **Step 3: SETUP.md** — 재현 명령 3개(Blender 스크립트, UE 파이썬 임포트·맵 생성, 영상 렌더)를 "M1 재현" 절로 추가한다.

- [ ] **Step 4: 커밋·푸시**

```bash
git add docs README.md
git commit -m "docs: record M1 results and reproduction commands"
git push -u origin feat/m1-scene
```

- [ ] **Step 5: superpowers:finishing-a-development-branch로 병합 결정**

---

## 범위 밖 (다음 계획)

- 산호·바위·부유 입자·후처리 색보정 — M5 품질 조정.
- 별명 입력 UI·세션·이름표(F-01~04, F-14 Unreal 계층) — M2.
- 방향키 제어·창 포커스(F-05~07 Unreal 계층) — M3.
- 클릭 도망(F-09~12 Unreal 계층) — M4.
- 성능 측정·패키징·UBT 앱 마무리 이슈 — M5.
