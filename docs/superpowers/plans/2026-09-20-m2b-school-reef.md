# M2b — 배경 물고기 군집과 산호초·바위 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 산호초 바다에 배경 물고기 36마리(5종)가 서로 다른 깊이·크기·속도로 유영하고 바닥에 산호·바위가 놓인 장면을 만들고, 프레임 시간을 측정해 기록한다.

**Architecture:** `fishlib`을 spec 딕셔너리 기반 `build_species()`로 데이터화한 뒤 새 종 3개를 추가한다. 산호는 Blender 절차 생성, 바위는 Poly Haven CC0. 배치는 `build_reef_m1.py`가 고정 시드로 전부 결정하고 `.umap`에 저장한다(런타임 스포너 없음). 성능은 개발 전용 커맨드라인 옵션으로 프레임 시간 CSV를 남겨 측정한다.

**Tech Stack:** Blender 5.2 (bpy), Unreal 5.8.2 (C++, editor Python), Catch2, ffmpeg, Poly Haven API.

**공통 명령** (저장소 루트):

- Blender: `/Applications/Blender.app/Contents/MacOS/Blender -b -P <script>`
- ctest: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure`
- UE 빌드 (두 번 실행): `"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -vi "\[upgrade\]" | grep -E "error|warning: |Result"`
- UE 테스트: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "Test Completed|LogAutomationController: Error"`
- UE 파이썬: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/<script>" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "_OK|Traceback|LogPython: Error"` (종료 코드가 아니라 `_OK` 마커로 판정)

**파일 구조(최종):**

```
assets/blender/
  fishlib.py              # + build_species(spec), axis_band_mask, mix_over, pec_stray_count
  make_bluetang.py        # spec만 남김 (산출물 불변)
  make_clownfish.py       # spec만 남김 (산출물 불변)
  make_yellowtang.py  make_butterflyfish.py  make_damselfish.py
  make_corals.py          # 가지·판·뇌 산호 3종
  export/ <종>.fbx, T_<종>_BaseColor.png, preview_<종>.png
  export/ BranchCoral.fbx, PlateCoral.fbx, BrainCoral.fbx, T_<산호>_BaseColor.png, preview_corals.png
assets/models/rocks/      # Poly Haven CC0 (fbx/gltf + 텍스처)
unreal/Aquarium/Scripts/
  import_fish.py          # 5종
  import_props.py         # 산호 3종 + 바위 → /Game/Props
  build_reef_m1.py        # 시드 기반 군집 36 + 소품 14
  verify_scene.py
unreal/Aquarium/Source/Aquarium/
  AquariumGameMode.h/.cpp # 카탈로그 5종
  DiverPlayerController.h/.cpp  # -AquariumFrameStats=<file>
  Tests/SessionTests.cpp, Tests/ControllerTests.cpp
scripts/measure_m2b_perf.sh, scripts/render_m2b_video.sh
docs/reviews/<날짜>-m2b-{reef.mp4,wide.png,school.png,perf.md}
```

---

### Task 0: 브랜치

- [ ] **Step 1**

```bash
git checkout main && git pull && git checkout -b feat/m2b-school-reef
```

---

### Task 1: `fishlib.build_species` 데이터화 — 기존 2종 산출물 불변

**Files:**
- Modify: `assets/blender/fishlib.py`, `assets/blender/make_bluetang.py`, `assets/blender/make_clownfish.py`
- Rename: `assets/blender/export/preview.png` → `preview_bluetang.png` (스크립트 출력 경로 변경으로)

- [ ] **Step 1: 기준선 기록** (두 종 모두)

```bash
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m2b_base && mkdir -p $B
cp assets/blender/export/T_BlueTang_BaseColor.png assets/blender/export/T_Clownfish_BaseColor.png $B/
for S in BlueTang Clownfish; do
/Applications/Blender.app/Contents/MacOS/Blender -b --python-expr "import bpy; bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.fbx(filepath='$PWD/assets/blender/export/$S.fbx'); a=[o for o in bpy.data.objects if o.type=='ARMATURE'][0]; m=[o for o in bpy.data.objects if o.type=='MESH'][0]; print('BASE_$S', sorted(b.name for b in a.data.bones), tuple(round(x,3) for x in m.dimensions), len(m.data.vertices))" 2>&1 | grep "BASE_$S"
done | tee $B/baseline.txt
```

- [ ] **Step 2: `fishlib`에 헬퍼 추가**

```python
def axis_band_mask(nt, axis_socket, center, half_width, edge=0.0):
    """0..1 mask: 1 inside |axis-center| <= half_width, 0 beyond, ramping over `edge`.
    Built as SUBTRACT -> ABSOLUTE -> MapRange with From Min > From Max (intentional inverted ramp)."""
    sub = nt.nodes.new("ShaderNodeMath"); sub.operation = 'SUBTRACT'; sub.inputs[1].default_value = center
    nt.links.new(axis_socket, sub.inputs[0])
    ab = nt.nodes.new("ShaderNodeMath"); ab.operation = 'ABSOLUTE'
    nt.links.new(sub.outputs[0], ab.inputs[0])
    rng = nt.nodes.new("ShaderNodeMapRange")
    rng.inputs["From Min"].default_value = half_width + edge
    rng.inputs["From Max"].default_value = half_width * 0.85 if edge == 0.0 else half_width
    nt.links.new(ab.outputs[0], rng.inputs["Value"])
    return rng.outputs[0]

def mix_over(nt, base_socket_or_color, mask_socket, color):
    """Mix RGBA: `color` over the base where mask is 1. Returns the Result socket."""
    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = 'RGBA'
    mix.inputs["B"].default_value = color
    if hasattr(base_socket_or_color, "links"):
        nt.links.new(base_socket_or_color, mix.inputs["A"])
    else:
        mix.inputs["A"].default_value = base_socket_or_color
    nt.links.new(mask_socket, mix.inputs["Factor"])
    return mix.outputs["Result"]

def position_axis(nt):
    """Geometry>Position -> SeparateXYZ. Returns the SeparateXYZ node (world space; body must be at the origin)."""
    geo = nt.nodes.new("ShaderNodeNewGeometry"); sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    return sep

def pec_stray_count(body, half_len, x_min=-0.05, x_max=0.45):
    """Vertices outside the pectoral bone's x window that are still dominated (>0.5) by PecL/PecR."""
    idx = {g.index for g in body.vertex_groups if g.name in ("PecL", "PecR")}
    n = 0
    for v in body.data.vertices:
        if any(g.group in idx and g.weight > 0.5 for g in v.groups):
            t = v.co.x / half_len
            if t < x_min or t > x_max: n += 1
    return n

def build_species(spec):
    """Runs the whole pipeline for one species. `spec` keys:
      name, body=(len, h, w, taper_z, taper_y), fin_thickness, fins=[(name, verts_fn, faces)],
      color_fn(nt, bsdf, ctx), rig={pec_z, tail_tip_x, pec_span_y, pec_drop_z}, cam_loc, root_dir, export_dir
    `verts_fn(ctx)` receives ctx = {L, body_h(x), BODY_H, BODY_W, PEC_ROOT_Y} and returns the vertex list.
    Returns dict(verts, bones, unweighted, root_max_w, pec_stray)."""
```

`build_species` 본문은 현재 `make_bluetang.py`의 단계 순서를 그대로 옮긴다: `reset_scene` → `build_body` → fins(`add_fin`) → `join_fins` → `unwrap` → `bake_base_color(color_fn)` → `build_rig` → `skin` → `assert_rig_contract` → `pec_stray_count` → `save_and_export` → `render_preview(preview_<name>.png)`.

- [ ] **Step 3: 두 종을 spec으로 교체** — `make_bluetang.py`는 수치(25/12/3, taper 0.55/0.4, 지느러미 표, 색 노드)를 그대로 spec에 넣고 `F.build_species(SPEC)` 한 줄을 호출한다. 색 노드는 새 헬퍼를 쓰되 **결과 그래프가 동일**해야 한다(블루탱의 기존 검은 띠는 `axis_band_mask(sep.outputs["Z"], 0, BODY_H*0.05, edge=BODY_H*0.23)`가 아니라 기존 수식과 같은 값을 내도록 맞춘다 — 값이 달라지면 베이크 바이트가 바뀌므로 Step 4에서 잡힌다). 출력 라인은 `BLUETANG_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d` 형식을 유지한다. 클라운피시도 동일.

- [ ] **Step 4: 불변 검증**

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_bluetang.py 2>&1 | grep -E "BLUETANG_OK|Traceback"
/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_clownfish.py 2>&1 | grep -E "CLOWNFISH_OK|Traceback"
# 위 Step 1의 BASE_ 명령을 다시 실행해 after.txt에 저장한 뒤
diff $B/baseline.txt $B/after.txt && echo UNCHANGED
cmp assets/blender/export/T_BlueTang_BaseColor.png $B/T_BlueTang_BaseColor.png && echo BT_TEXTURE_IDENTICAL
cmp assets/blender/export/T_Clownfish_BaseColor.png $B/T_Clownfish_BaseColor.png && echo CF_TEXTURE_IDENTICAL
```

Expected: `UNCHANGED`, 두 개의 `*_TEXTURE_IDENTICAL`. 텍스처가 다르면 색 그래프가 달라진 것이므로 헬퍼 파라미터를 고친다(수치를 바꾸지 않는다). 프리뷰 파일명은 `preview_bluetang.png`/`preview_clownfish.png`로 통일하고 `docs/ASSETS.md`의 파일명 언급을 갱신한다.

- [ ] **Step 5: 커밋**

```bash
git add assets/blender docs/ASSETS.md
git commit -m "refactor: data-driven fishlib.build_species; existing species outputs unchanged"
```

---

### Task 2: 새 물고기 3종

**Files:**
- Create: `assets/blender/make_yellowtang.py`, `make_butterflyfish.py`, `make_damselfish.py`
- Modify: `docs/ASSETS.md`

- [ ] **Step 1: 세 스크립트 작성** — 각각 `F.build_species(SPEC)` 한 번 호출. 수치:

| 종 | body=(len,h,w,taper_z,taper_y) | fin_thickness | rig | cam_loc | 색 |
|---|---|---|---|---|---|
| YellowTang | (20, 10, 2.6, 0.55, 0.4) | 0.2 | pec_z 0.4, tail_tip_x −L−5, pec_span_y 3.2, pec_drop_z 2.0 | (17, −38, 8) | 전체 (0.95,0.75,0.05); 꼬리자루(−L·0.75 부근, half_width 1.2)만 흰색 (0.95,0.95,0.9) |
| Butterflyfish | (14, 10.5, 2.4, 0.5, 0.42) | 0.15 | pec_z 0.35, tail_tip_x −L−3.2, pec_span_y 2.4, pec_drop_z 1.4 | (12, −28, 6) | 바탕 (0.95,0.8,0.15); 눈띠: x=+L·0.62, half_width 0.8, 검정 (0.03,0.02,0.02); 꼬리 근처 띠: x=−L·0.55, half_width 0.7, 검정 |
| Damselfish | (7, 3.4, 1.6, 0.5, 0.45) | 0.12 | pec_z 0.25, tail_tip_x −L−1.8, pec_span_y 1.4, pec_drop_z 0.8 | (7, −16, 3.5) | 바탕 (0.05,0.15,0.6); 꼬리 쪽(x < −L·0.4) 밝은 청색 (0.25,0.6,0.95) |

지느러미 표는 클라운피시 것을 비율로 스케일해 쓴다(모두 `ctx["body_h"]` 기반, 절대 cm 대신 `L`/`BODY_H` 비율로 쓴다). 출력: `<SPECIES>_OK verts=... bones=10 unweighted=0 rootMaxW=0.000 pecStray=<n>`.

- [ ] **Step 2: 실행과 검증**

```bash
for S in yellowtang butterflyfish damselfish; do
  /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_$S.py 2>&1 | grep -E "_OK|Traceback"
done
for S in YellowTang Butterflyfish Damselfish; do
/Applications/Blender.app/Contents/MacOS/Blender -b --python-expr "import bpy; bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.fbx(filepath='$PWD/assets/blender/export/$S.fbx'); a=[o for o in bpy.data.objects if o.type=='ARMATURE'][0]; m=[o for o in bpy.data.objects if o.type=='MESH'][0]; print('FBX_$S', sorted(b.name for b in a.data.bones), tuple(round(x,3) for x in m.dimensions))" 2>&1 | grep "FBX_$S"
done
```

Expected: 세 종 모두 `bones=10`, `unweighted=0`, `rootMaxW=0.000`, `pecStray` ≤ 10; FBX 본 목록이 `['PecL','PecR','Root','Spine0'..'Spine5','Tail']`. 각 `preview_<species>.png`를 Read 도구로 보고 형태·색을 확인한다(노란탱: 노란 타원+흰 꼬리자루 / 나비고기: 높은 몸통+검은 띠 2개 / 담셀: 작은 파란 몸통+밝은 꼬리). 어긋나면 수치를 고치고 반복(최대 4회).

- [ ] **Step 3: ASSETS.md** — 직접 제작 표에 세 행 추가(도구 Blender 5.2.2 LTS, 스크립트 경로, 산출물, 외부 텍스처 없음).

- [ ] **Step 4: 커밋**

```bash
git add assets/blender docs/ASSETS.md
git commit -m "feat: three more scripted fish species (yellow tang, butterflyfish, damselfish)"
```

---

### Task 3: 절차 생성 산호 3종

**Files:**
- Create: `assets/blender/make_corals.py`
- Modify: `docs/ASSETS.md`

- [ ] **Step 1: 스크립트** — 하나의 스크립트가 세 메시를 각각 만들고 내보낸다. 공통: `F.reset_scene()`, 절차 색 노드 → `F.bake_base_color(..., size=1024)`, `bpy.ops.export_scene.fbx(..., use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='X', axis_up='Z', add_leaf_bones=False, bake_anim=False, mesh_smooth_type='FACE', path_mode='AUTO')` (아마추어 없음).

```python
# assets/blender/make_corals.py
# Procedural coral props (no rig): branch, plate and brain coral.
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_corals.py
import os, sys, math, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy, bmesh
import fishlib as F

def build_branch(rng, height=70.0):
    """Recursive tapered cylinders, depth 3. Returns the joined object."""
    parts = []
    def branch(origin, direction, length, radius, depth):
        bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=radius, depth=length)
        ob = bpy.context.object
        ob.rotation_mode = 'QUATERNION'
        ob.rotation_quaternion = direction.to_track_quat('Z', 'Y')
        ob.location = origin + direction * (length / 2)
        parts.append(ob)
        if depth == 0: return
        tip = origin + direction * length
        for _ in range(rng.randint(2, 3)):
            d = (direction + mathutils.Vector((rng.uniform(-0.7, 0.7), rng.uniform(-0.7, 0.7), rng.uniform(0.1, 0.5)))).normalized()
            branch(tip, d, length * rng.uniform(0.55, 0.75), radius * 0.65, depth - 1)
    import mathutils
    branch(mathutils.Vector((0, 0, 0)), mathutils.Vector((0, 0, 1)), height * 0.45, 3.2, 3)
    # join
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    ob = bpy.context.object; ob.name = "BranchCoral"
    bpy.ops.object.shade_smooth()
    return ob
```

판 산호: `primitive_cylinder_add(vertices=48, radius=50, depth=4)` 후 정점 z를 `sin(2·atan2(y,x)) * 6 * (r/50)` 만큼 올려 물결, 가장자리 반경을 노이즈로 흔든다. 뇌 산호: `primitive_uv_sphere_add(radius=30)` 후 z<0 정점을 눌러 반구로 만들고, 표면에 `noise` 기반 변위(홈)를 준다. 세 메시 모두 `smart_project` UV, 색: 가지=주황빛 분홍(0.9,0.35,0.45), 판=베이지(0.85,0.7,0.5), 뇌=연보라(0.75,0.6,0.8), 각각 노이즈로 명암 변이를 섞는다. 프리뷰는 세 개를 나란히 놓고 한 장(`preview_corals.png`).

출력: `CORALS_OK branch=<verts> plate=<verts> brain=<verts>`.

- [ ] **Step 2: 실행과 확인**

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_corals.py 2>&1 | grep -E "CORALS_OK|Traceback|line [0-9]+"
ls -la assets/blender/export/{BranchCoral,PlateCoral,BrainCoral}.fbx assets/blender/export/preview_corals.png
```

Expected: `CORALS_OK`, 각 메시 정점 수 300~4000, 세 FBX 존재. `preview_corals.png`를 Read 도구로 보고 세 형태가 구분되는지 확인. 재귀 분기가 폭주하면 `depth`/개수를 줄인다.

- [ ] **Step 3: ASSETS.md 행 추가, 커밋**

```bash
git add assets/blender/make_corals.py assets/blender/export docs/ASSETS.md
git commit -m "feat: procedural branch, plate and brain coral props"
```

---

### Task 4: Poly Haven CC0 바위 모델

**Files:**
- Create: `assets/models/rocks/`
- Modify: `docs/ASSETS.md`, `.gitattributes` (필요 시 `*.gltf`, `*.bin`, `*.blend`)

- [ ] **Step 1: 조회와 다운로드**

```bash
curl -s "https://api.polyhaven.com/assets?t=models&c=rock" | python3 -c "import sys,json; d=json.load(sys.stdin); print('\n'.join(f'{k}: {v[\"name\"]}' for k,v in list(d.items())[:15]))"
```

이름에 "rock"/"boulder"가 들어간 id 2~3개를 고르고, 각각:

```bash
curl -s "https://api.polyhaven.com/files/<id>" | python3 -c "import sys,json; f=json.load(sys.stdin); print(json.dumps(list(f.keys()))); print(json.dumps(list(f.get('fbx',{}).keys()) or list(f.get('gltf',{}).keys())))"
```

가능하면 `fbx`의 `1k`(또는 가장 작은 해상도) 항목을, 없으면 `gltf`를 받아 `assets/models/rocks/<id>/`에 저장한다(텍스처 포함 `include` 항목도 함께). `file`로 실제 형식을 확인한다. Poly Haven은 전부 CC0다(https://polyhaven.com/license).

- [ ] **Step 2: ASSETS.md** — 바위마다 한 행(저작자는 `https://api.polyhaven.com/info/<id>`의 `authors`).

- [ ] **Step 3: 커밋**

```bash
git add .gitattributes assets/models docs/ASSETS.md
git commit -m "chore: add Poly Haven CC0 rock models with provenance"
```

네트워크가 막히거나 모델 카테고리에 적당한 바위가 없으면 **BLOCKED로 보고하지 말고** 바위도 절차 생성으로 대체한다(`make_corals.py`에 `build_rock(rng)` 추가: 구 + 노이즈 변위 + flat shading). 그 경우 ASSETS.md에는 직접 제작으로 기록하고 계획의 이 태스크에 결정을 적는다.

---

### Task 5: 소품 임포트 스크립트

**Files:**
- Create: `unreal/Aquarium/Scripts/import_props.py`
- 산출물: `unreal/Aquarium/Content/Props/{SM_BranchCoral, SM_PlateCoral, SM_BrainCoral, SM_Rock*, M_*, T_*}.uasset`

- [ ] **Step 1: 스크립트** — `import_fish.py`의 구조를 따르되 스태틱 메시로: `unreal.FbxImportUI` with `import_as_skeletal=False`, `mesh_type_to_import=FBXIT_STATIC_MESH`, `import_materials=False`, `import_textures=False`, `static_mesh_import_data.combine_meshes=True`. 각 산호는 베이크 PNG를 텍스처로 임포트하고 `M_<name>`(TextureSample→BaseColor, Constant 0.6→Roughness, `used_with_static_lighting` 불필요)을 만들어 슬롯에 지정. 바위는 다운로드한 텍스처(diffuse/normal/rough가 있으면)를 연결하고, 없으면 회색 상수 머티리얼. idempotent(재실행 시 중복 없음). 출력: `PROPS_OK count=<n> assets=[...]`.

- [ ] **Step 2: 실행 (두 번)** — UE 파이썬 `import_props.py`. Expected `PROPS_OK count=5` 이상, `ls unreal/Aquarium/Content/Props/`에 `_1` 중복 없음.

- [ ] **Step 3: 커밋**

```bash
git add unreal/Aquarium/Scripts/import_props.py unreal/Aquarium/Content/Props
git commit -m "feat: scripted import of coral and rock props"
```

---

### Task 6: 5종 임포트와 카탈로그 확장

**Files:**
- Modify: `unreal/Aquarium/Scripts/import_fish.py` (기본 종 목록 5개), `Source/Aquarium/AquariumGameMode.cpp` (카탈로그 5종), `Source/Aquarium/Tests/SessionTests.cpp` (도달성 테스트)

- [ ] **Step 1: 테스트 먼저** — `SessionTests.cpp`의 `SpawnGameMode(World, Seed, SpeciesCount)`가 최대 5종까지 만들도록 확장하고, `FSessionSeedDeterminesSpecies`를 5종 기준으로 바꾼다: `TestEqual(TEXT("five species loaded"), A->LoadedSpeciesCount(), 5);` 와 시드 1..60에서 **다섯 종이 모두 배정되는지**(`TSet<int32> Seen`) 확인. 빌드 → 실패 확인(5종 메시가 아직 임포트 전이면 `LoadedSpeciesCount()`가 2).

- [ ] **Step 2: 임포트** — `import_fish.py`의 기본 목록을 `["BlueTang","Clownfish","YellowTang","Butterflyfish","Damselfish"]`로 바꾸고 UE 파이썬으로 실행. Expected: `IMPORT_OK species=...` 5줄, 각 본 목록 동일. 두 번 실행해 중복 없음 확인.

- [ ] **Step 3: 카탈로그** — `AAquariumGameMode` 생성자의 기본 카탈로그에 세 종을 추가(`NSLOCTEXT` 표시 이름: "노란탱", "나비고기", "담셀피시"; 소프트 경로 `/Game/Fish/<Name>/SK_<Name>.SK_<Name>`).

- [ ] **Step 4: 통과 확인** — UE 빌드 두 번 → 테스트. Expected 24개 Success(테스트 수는 그대로, 내용만 5종 기준).

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Scripts/import_fish.py unreal/Aquarium/Content/Fish unreal/Aquarium/Source
git commit -m "feat: five-species catalog and import"
```

---

### Task 7: 군집·소품 배치와 검증

**Files:**
- Modify: `unreal/Aquarium/Scripts/build_reef_m1.py`, `unreal/Aquarium/Scripts/verify_scene.py`

- [ ] **Step 1: 배치 코드** — `build_reef_m1.py`의 `BACKGROUND_FISH` 표를 시드 생성으로 교체:

```python
SCHOOL_SEED = 20260920
SCHOOL_COUNT = 36
# (mesh path, weight) — the small damselfish fills the background
SCHOOL_SPECIES = [
    ("/Game/Fish/Damselfish/SK_Damselfish",       0.40),
    ("/Game/Fish/BlueTang/SK_BlueTang",           0.15),
    ("/Game/Fish/Clownfish/SK_Clownfish",         0.15),
    ("/Game/Fish/YellowTang/SK_YellowTang",       0.15),
    ("/Game/Fish/Butterflyfish/SK_Butterflyfish", 0.15),
]
SCHOOL_X = (330.0, 700.0)      # behind the player's plane at X = 220
SCHOOL_Y = (-250.0, 250.0)
SCHOOL_Z = (110.0, 200.0)
SCHOOL_HALF_W = (120.0, 220.0)
SCHOOL_HALF_H = (40.0, 60.0)
SCHOOL_SCALE = (0.75, 1.3)
SCHOOL_SPEED = (25.0, 55.0)

PROP_SEED = 77
PROP_COUNT = 14
PROP_MESHES = ["/Game/Props/SM_BranchCoral", "/Game/Props/SM_PlateCoral", "/Game/Props/SM_BrainCoral"]  # + rocks found in /Game/Props
PROP_X = (150.0, 800.0)
PROP_Y = (-400.0, 400.0)
PROP_SCALE = (0.7, 1.4)
PROP_CLEAR_RADIUS_Y = 60.0     # keep the camera's forward lane clear
PROP_CLEAR_X = 320.0
```

스폰 루프는 `random.Random(SCHOOL_SEED)`로 종을 가중 추출(`random.choices`)하고 각 값을 균등 추출해 `AFishActor`를 만든 뒤 `seed`, `plane_origin`, `plane_half_width/height`, `max_speed`, 액터 스케일, `FISH_EDITOR_YAW`, 스킨 에셋 푸시를 설정한다. 소품은 `random.Random(PROP_SEED)`로 메시·위치·yaw·스케일을 뽑아 `StaticMeshActor`로 스폰하되, `abs(y) < PROP_CLEAR_RADIUS_Y and x < PROP_CLEAR_X`이면 y를 부호 유지한 채 `PROP_CLEAR_RADIUS_Y` 바깥으로 밀어낸다. 출력: `REEF_OK actors=<n> fish=36 props=14`.

- [ ] **Step 2: 검증 스크립트** — `verify_scene.py`에 추가: FishActor 수 == 36, `is_player_fish` 전부 False, 서로 다른 메시가 5개, 모든 `plane_origin.x >= 330`, 액터 스케일 0.75~1.3, StaticMeshActor 중 바닥(SandFloor)을 제외한 소품 수 == 14, 소품 중 카메라 정면 레인(|y|<60 and x<320) 없음. 출력 `SCENE_OK actors=<n> fish=36 props=14 species=5`.

- [ ] **Step 3: 실행** — UE 파이썬 `build_reef_m1.py` → `REEF_OK`; `verify_scene.py` → `SCENE_OK`. 두 번 반복해 동일 출력·중복 에셋 없음 확인.

- [ ] **Step 4: 커밋**

```bash
git add unreal/Aquarium/Scripts unreal/Aquarium/Content/Maps
git commit -m "feat: seeded 36-fish school and 14 reef props in ReefM1"
```

---

### Task 8: 프레임 시간 측정 옵션

**Files:**
- Modify: `Source/Aquarium/DiverPlayerController.h/.cpp`, `Source/Aquarium/Tests/ControllerTests.cpp`
- Create: `scripts/measure_m2b_perf.sh`

- [ ] **Step 1: 테스트 먼저** — `ControllerTests.cpp`에 `Aquarium.Controller.ParsesFrameStatsPath`: 정적 `static bool ParseFrameStatsPath(const TCHAR* CmdLine, FString& OutPath)`가 `-AquariumFrameStats=/tmp/x.csv` → true/경로, 없으면 false, 빈 값이면 false. 빌드 → 컴파일 실패(RED).

- [ ] **Step 2: 구현** — `#if !UE_BUILD_SHIPPING` 안에서 `StartFrameStatsIfRequested()`(BeginPlay에서 호출)가 경로를 받아 `TArray<float> FrameTimes`를 예약하고, `Tick`에서 `FrameTimes.Add(DeltaSeconds)`(상한 200k), `EndPlay`에서 CSV(`frame,delta_seconds` 헤더 + 행)를 `FFileHelper::SaveStringToFile`로 쓴다. 파일 경로 로그는 남기되 별명은 절대 남기지 않는다.

- [ ] **Step 3: 측정 스크립트** — `scripts/measure_m2b_perf.sh`: 에디터를 `-game -windowed -ResX=1920 -ResY=1080 -ForceRes -AquariumAutoNickname=측정 -AquariumAssignmentSeed=1 -AquariumFrameStats="$OUT.csv" -nosplash` 로 95초 띄우고(백그라운드 + `sleep 95` + `pkill -x UnrealEditor`), CSV에서 **첫 30초를 버리고** 나머지로 평균 fps와 95백분위 프레임 시간(ms)을 python3로 계산해 `docs/reviews/<날짜>-m2b-perf.md`에 표로 쓴다(기록: 해상도, 물고기 수, 소품 수, 커밋 해시). 출력 `PERF_OK avg_fps=<x> p95_ms=<y>`.

- [ ] **Step 4: 통과 확인** — UE 빌드 두 번 → 테스트 25개 Success → `scripts/measure_m2b_perf.sh` 실행 → `PERF_OK`. 프레임이 300개 미만이면 창이 뜨지 않은 것이므로 로그를 확인한다.

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Source scripts/measure_m2b_perf.sh docs/reviews
git commit -m "feat: dev-only frame-time CSV and M2b performance measurement"
```

---

### Task 9: 검토 영상과 스틸

**Files:**
- Create: `scripts/render_m2b_video.sh`
- 산출물: `docs/reviews/<날짜>-m2b-{reef.mp4,wide.png,school.png}`

- [ ] **Step 1: 스크립트** — `scripts/render_m2_video.sh`를 복사해 `-seconds=22`, `-AquariumAutoNickname=니모 -AquariumAssignmentSeed=1`(나가기 없음), 출력 `<날짜>-m2b-reef.mp4`, 스틸 2장(`-ss 4` → `-m2b-wide.png`, `-ss 16` → `-m2b-school.png`). UI 포함 캡처(`-AquariumCaptureUI`)를 그대로 쓴다.

- [ ] **Step 2: 실행과 확인** — 스틸 2장과 12프레임 컨택트 시트를 Read 도구로 본다. 기대: 산호·바위가 바닥에 흩어져 있고 물고기 30마리 이상이 서로 다른 크기·깊이로 유영, 내 물고기가 가장 앞에서 가장 크고 이름표가 따라다님, 프레임 간 튐 없음. 물고기가 산호를 자주 관통하거나 화면이 너무 붐비면 `SCHOOL_COUNT`가 아니라 **배치 범위**(`SCHOOL_Y`, `SCHOOL_Z`)를 넓혀 조정하고 재실행한다(최대 3회).

- [ ] **Step 3: 커밋**

```bash
git add scripts/render_m2b_video.sh docs/reviews
git commit -m "feat: M2b reef capture script and review video/stills"
```

---

### Task 10: 문서와 마무리

- [ ] **Step 1: 전체 재검증** — ctest(60), UE 빌드, UE 테스트(25), `verify_scene.py`, `PERF_OK` 수치를 모두 새로 확보한다.
- [ ] **Step 2: 문서** — `docs/TASK.md` M2b 행(테스트 수, 영상·성능 경로, 사용자 검토 대기), `docs/SETUP.md` 재현 절에 `make_corals.py`·`import_props.py`·`measure_m2b_perf.sh`·`render_m2b_video.sh` 추가, `README.md` 현재 상태, `docs/ASSETS.md` 최종 확인.
- [ ] **Step 3: 커밋·푸시**

```bash
git add docs README.md
git commit -m "docs: record M2b results and reproduction commands"
git push -u origin feat/m2b-school-reef
```

- [ ] **Step 4: 사용자에게 영상·스틸·성능 요약 전달 후 superpowers:finishing-a-development-branch**

---

## 범위 밖 (다음 계획)

- M3: 방향키 조종(F-05~07), 창 포커스, 규칙 계층 벽 조향 개선.
- M4: 실사 반복(물고기·산호 정밀화, 무리 행동, 소품 충돌), 수직 전환 롤 제거.
- M5: 클릭 도망(F-09~13). M6: 성능 판정·패키징.
