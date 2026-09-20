# M4a — 물고기 정밀화 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 다섯 종의 물고기를 프로파일 곡선 몸통 + 눈 + 막 지느러미 + 비늘 노멀/러프니스 맵으로 다시 만들어, 같은 카메라에서 "장난감"이 아니라 "물고기"로 읽히게 한다. 본·축·단위 계약과 기존 테스트는 전부 유지한다.

**Architecture:** `fishlib`에 로프트 몸통·눈·막 지느러미·다중 맵 베이크를 추가하고, 종 스크립트는 여전히 spec 한 덩어리 + `build_species` 한 줄로 남긴다. Unreal 쪽은 임포트 스크립트가 세 장의 맵을 연결한 서브서피스 머티리얼을 만든다. 판정은 개선 전/후 나란히 스틸과 성능 재측정.

**Tech Stack:** Blender 5.2 (bpy, Cycles 베이크), Unreal 5.8.2 (에디터 Python, C++ 변경 없음), ffmpeg.

**공통 명령**:
- Blender: `/Applications/Blender.app/Contents/MacOS/Blender -b -P <script>` (종당 1~3분)
- FBX 계약 확인: `/Applications/Blender.app/Contents/MacOS/Blender -b --python-expr "import bpy; bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.fbx(filepath='$PWD/assets/blender/export/<S>.fbx'); a=[o for o in bpy.data.objects if o.type=='ARMATURE'][0]; m=[o for o in bpy.data.objects if o.type=='MESH'][0]; print('FBX_<S>', sorted(b.name for b in a.data.bones), len(m.data.vertices), tuple(round(x,3) for x in m.dimensions))"`
- UE 파이썬: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/<s>.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "_OK|Traceback|LogPython: Error"`
- UE 빌드(두 번) / 테스트(37개): M3 계획과 동일

---

### Task 0: 브랜치

- [ ] **Step 1**

```bash
git checkout main && git pull && git checkout -b feat/m4a-fish-fidelity
```

- [ ] **Step 2: 기준 보관** — 개선 전 비교용으로 현재 산출물을 스크래치에 복사한다.

```bash
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m4a_before && mkdir -p $B
cp assets/blender/export/preview_*.png $B/
cp docs/reviews/2026-09-21-m3-steer.png $B/before-scene.png
```

---

### Task 1: 프로파일 곡선 몸통 (`fishlib.build_body_profile`)

**Files:** Modify `assets/blender/fishlib.py`; 검증용 임시 스크립트는 만들지 않는다(다음 태스크에서 종이 쓴다).

- [ ] **Step 1: 함수 추가**

```python
def build_body_profile(name, length, profile, belly, width, sections=28, ring_segments=20):
    """Lofted fish body from silhouette functions instead of a scaled sphere.

    t runs 0 (nose, +X) to 1 (tail tip, -X). profile(t) and belly(t) return the height above and
    below the spine in cm; width(t) returns the half width in cm. Sections are elliptical but
    vertically asymmetric (back and belly differ), which is what makes the silhouette read as a
    fish rather than a capsule. The nose and tail rings collapse to a point so the mesh is closed.
    Precondition: object mode, scene reset. Returns the body object (selected, active)."""
```
구현: `bmesh`로 각 섹션의 링을 만들고 `bmesh.ops.bridge_loops`로 잇거나, 정점을 직접 만들고 면을 연결한다. x = `length/2 - t*length`. 각 링의 정점은 각도 `a`에 대해 `z = (a가 위쪽이면 profile(t) 아니면 -belly(t)) * sin` 형태로 상하를 비대칭 보간하고, `y = width(t) * cos`. 첫/마지막 링은 한 점으로 수렴. 마지막에 `shade_smooth`.

- [ ] **Step 2: 형태 확인용 임시 렌더** — 블루탱 수치로 한 번 호출해 프리뷰를 뽑고 Read 도구로 본다(등이 솟고 배가 완만하며 머리가 둥글고 꼬리가 얇은지). 확인 후 임시 스크립트는 삭제한다.

- [ ] **Step 3: 커밋**

```bash
git add assets/blender/fishlib.py
git commit -m "feat: lofted profile-curve fish body in fishlib"
```

---

### Task 2: 눈과 막 지느러미 (`fishlib`)

**Files:** Modify `assets/blender/fishlib.py`

- [ ] **Step 1: `add_eye`**

```python
def add_eye(name, centre, radius, sink=0.0):
    """Eyeball sphere placed at `centre` (cm, body space) and pushed `sink` cm toward the body
    axis so it sits in the socket. Joined into the body later; shares the body's UV space so the
    iris/pupil come from the baked colour map."""
```
좌우는 호출자가 `y` 부호를 바꿔 두 번 호출한다.

- [ ] **Step 2: `add_membrane_fin`**

```python
def add_membrane_fin(name, outline, thickness_root, thickness_edge, rays=0, ray_depth=0.0):
    """Fin membrane whose thickness tapers from `thickness_root` at the first outline vertex to
    `thickness_edge` at the outer rim, optionally ridged by `rays` fin rays of `ray_depth` cm.
    `outline` is a list of (x, z) in body space; the fin lies in the XZ plane (y = 0) and is
    mirrored by Solidify. Flat plates read as paper; the taper and the rays are what make a fin."""
```
구현: 외곽선을 따라 정점을 만들고, 루트→가장자리 보간으로 각 정점에 두께를 준다(Solidify 대신 직접 두 겹을 만들고 가장자리에서 붙인다). `rays > 0`이면 루트에서 가장자리로 뻗는 능선을 `ray_depth`만큼 돌출시킨다.

- [ ] **Step 3: 확인** — Task 1과 같은 방식으로 지느러미 하나를 렌더해 두께 구배와 살이 보이는지 확인.

- [ ] **Step 4: 커밋**

```bash
git add assets/blender/fishlib.py
git commit -m "feat: eyeball and tapered membrane fins in fishlib"
```

---

### Task 3: 다중 맵 베이크 (`fishlib.bake_maps`, `scale_pattern`)

**Files:** Modify `assets/blender/fishlib.py`

- [ ] **Step 1: `scale_pattern`**

```python
def scale_pattern(nt, cell_size, sharpness=3.0):
    """Voronoi-based scale pattern. Returns (height_socket, roughness_socket) in 0..1; feed the
    height into a Bump node for the normal bake and the roughness into the Principled input."""
```

- [ ] **Step 2: `bake_maps`**

```python
def bake_maps(body, mat_name, build_nodes, base_name, export_dir, size=2048, samples=16):
    """Bakes base colour, normal and roughness to <export_dir>/T_<base_name>_{BaseColor,Normal,
    Roughness}.png and rewires the baked textures into the material. build_nodes(nt, bsdf, ctx)
    wires Base Color, Roughness and a Bump/Normal input; ctx carries the helpers. Raises if a
    baked map is degenerate (a normal map whose pixel variance is ~0 means nothing was baked)."""
```
구현: `bake_base_color`의 흐름을 따르되 세 번 베이크한다(`type='DIFFUSE'` 색만, `type='NORMAL'`, `type='ROUGHNESS'`). 각 이미지에 맞는 `ShaderNodeTexImage`를 활성 노드로 두고 베이크한다. 저장 후 노멀/러프니스는 `image.colorspace_settings.name = 'Non-Color'`.

- [ ] **Step 3: 커밋**

```bash
git add assets/blender/fishlib.py
git commit -m "feat: base colour, normal and roughness bake with scale pattern"
```

---

### Task 4: 다섯 종 재작성

**Files:** Modify `assets/blender/make_{bluetang,clownfish,yellowtang,butterflyfish,damselfish}.py`; `fishlib.build_species`가 새 경로를 쓰도록 수정

- [ ] **Step 1: `build_species` 갱신** — spec에 `profile`/`belly`/`width`/`eye`/`fins`(외곽선·살)/`scale_cell`이 있으면 새 경로(`build_body_profile` + `add_eye` + `add_membrane_fin` + `bake_maps`)를 쓰고, 없으면 기존 경로를 쓴다. 산호 스크립트는 영향을 받지 않는다.

- [ ] **Step 2: 종별 수치** — 각 종의 실제 비율을 반영한다. 예(블루탱): `profile = 등이 몸 중앙 앞쪽에서 최고, 눈 뒤로 급히 솟음`, `belly = 완만`, `width = 머리에서 최대, 꼬리자루에서 0.15배`. 종마다 눈 위치는 `t ≈ 0.12`, 반경은 몸 높이의 0.08배. 지느러미 외곽선은 기존 표를 비율로 유지하되 등·뒷지느러미는 살 6~10개, 꼬리는 8~12개.

- [ ] **Step 3: 실행과 계약 검증** — 다섯 종을 빌드하고 각각:
  - `<SPECIES>_OK verts=<n> bones=10 unweighted=0 rootMaxW=0.000 pecStray=<n>`에서 `verts <= 5000`
  - FBX 재임포트로 본 목록·치수 확인
  - 세 장의 맵이 2048²로 존재하고 노멀맵 픽셀 분산이 0이 아님
  - `preview_<species>.png`를 Read 도구로 보고 눈·비늘·지느러미 살이 보이는지 확인
  어긋나면 수치를 고쳐 종당 최대 4회 반복.

- [ ] **Step 4: ASSETS.md** — 산출물 목록에 노멀·러프니스 맵을 추가.

- [ ] **Step 5: 커밋**

```bash
git add assets/blender docs/ASSETS.md
git commit -m "feat: five species rebuilt with profile bodies, eyes, membrane fins and PBR maps"
```

---

### Task 5: Unreal 임포트와 재질

**Files:** Modify `unreal/Aquarium/Scripts/import_fish.py`

- [ ] **Step 1: 임포트 확장** — 종마다 세 장을 임포트(노멀은 sRGB off + `TC_NORMALMAP`, 러프니스는 sRGB off)하고 `M_<name>`을 다시 구성한다: 베이스 컬러, 노멀, 러프니스, `Subsurface` 셰이딩 모델 + 서브서피스 컬러(베이스 컬러 × 0.6) + `Opacity` 상수 0.15, 스페큘러 0.4. `used_with_skeletal_mesh`는 유지. 출력에 연결된 맵 목록을 포함한다.

- [ ] **Step 2: 실행(두 번)** — `IMPORT_OK species=... maps=[BaseColor,Normal,Roughness] ...` 다섯 줄, 중복 에셋 없음.

- [ ] **Step 3: 통과 확인** — UE 빌드 두 번 → Automation 37개 Success 유지(메시가 바뀌었으므로 `SpineBonesExistOnMesh`, `BodyWaveIsChained`, `TagClearsScaledBody`가 특히 중요하다).

- [ ] **Step 4: 커밋**

```bash
git add unreal/Aquarium/Scripts/import_fish.py unreal/Aquarium/Content/Fish
git commit -m "feat: PBR fish materials with normal, roughness and subsurface"
```

---

### Task 6: 비교 스틸·영상과 성능

**Files:** Create `scripts/render_m4a_compare.sh`; 산출물 `docs/reviews/<날짜>-m4a-{closeup.png,scene.png,compare.png,reef.mp4,perf.md}`

- [ ] **Step 1: 근접 스틸** — 개발 전용 옵션 없이, `build_reef_m1.py`를 건드리지 않고 별도 스크립트가 카메라를 물고기 1 m 앞에 두고 한 장 찍는다. 가장 단순한 방법은 에디터 Python으로 임시 레벨에 물고기 한 마리와 카메라를 놓고 MCP `CaptureViewport`를 쓰는 것이다(M1에서 검증된 경로, `docs/SETUP.md` 참고). 근접 스틸은 눈·비늘·지느러미 살이 보이는지 판단하는 용도다.
- [ ] **Step 2: 장면 스틸·영상** — `scripts/render_m3_video.sh`와 같은 인자로 `<날짜>-m4a-reef.mp4`와 `-scene.png`(t=12)를 만든다.
- [ ] **Step 3: 나란히 비교** — `ffmpeg -i before-scene.png -i <날짜>-m4a-scene.png -filter_complex hstack` 으로 `-compare.png`를 만들고 Read 도구로 본다. 개선이 보이지 않으면 수치를 조정해 Task 4로 돌아간다(최대 2회).
- [ ] **Step 4: 성능** — `scripts/measure_m2b_perf.sh`를 그대로 실행해 `docs/reviews/<날짜>-m4a-perf.md`를 만들고 M2b 수치(평균 102.5 fps, p95 10.22 ms)와 비교해 적는다. 평균 60 fps 미만이면 폴리곤/텍스처를 줄이고 재측정.
- [ ] **Step 5: 커밋**

```bash
git add scripts docs/reviews
git commit -m "feat: M4a comparison stills, reef clip and performance re-measurement"
```

---

### Task 7: 문서와 마무리

- [ ] **Step 1: 전체 재검증** — ctest(66), UE 빌드, Automation(37), 성능 수치.
- [ ] **Step 2: 문서** — `docs/TASK.md`에 M4a 행(M4를 a/b/c로 쪼갠 사실 포함), `docs/SETUP.md` 재현 명령, `README.md` 상태, `docs/ASSETS.md` 확인.
- [ ] **Step 3: 커밋·푸시**

```bash
git add docs README.md
git commit -m "docs: record M4a results and the M4 split"
git push -u origin feat/m4a-fish-fidelity
```

- [ ] **Step 4: 사용자에게 비교 스틸·근접 스틸·영상 전달 후 superpowers:finishing-a-development-branch**

---

## 범위 밖 (다음 계획)

- M4b: 산호·바위 품질, 조명·후처리(색보정·심도·부유 입자).
- M4c: 무리(보이즈) 행동, 소품 충돌, 수직 전환 롤 제거.
- M5 클릭 도망, M6 성능 판정·패키징.
