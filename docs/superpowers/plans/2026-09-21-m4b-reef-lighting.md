# M4b — 산호·바위 품질과 조명·후처리 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 산호를 물고기와 같은 PBR 3장(BaseColor/Normal/Roughness)으로 다시 굽고 5종으로 늘리며, 프롭 22개에 회전·기울임·비균일 스케일·틴트 변형을 준다. 조명은 보이지 않는 수면 차폐물로 빛줄기를 만들고 안개를 낮춰 20 m까지 색이 남게 한다. 후처리는 색보정·블룸·AO를 켜고 심도는 명시적으로 끈다. 부유 입자는 Niagara가 아니라 해석적 머티리얼 커튼 3장으로 만든다. 기존 규칙 66개·Automation 37개는 전부 유지한다.

**Architecture:** 에셋은 Blender `bpy`(`fishlib` 확장 + `make_corals.py`), 콘텐츠는 Unreal 에디터 Python(`import_props.py`, `build_reef_m1.py`). `.uasset`은 손으로 만지지 않는다. 판정은 M4a 캡처와의 나란히 스틸 + 성능 재측정.

**Tech Stack:** Blender 5.2 (bpy, Cycles 베이크), Unreal 5.8.2 (에디터 Python + C++ 자동화 테스트 1개), ffmpeg.

**공통 명령**:
- Blender: `/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_corals.py` (약 3~5분)
- UE 파이썬: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/<s>.py" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "_OK|Traceback|LogPython: Error|Failed to compile Material"`
- UE 빌드(두 번): `"$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex`
- Automation: `"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep "Test Completed"`

**모든 태스크에 걸리는 규약 두 가지 (M4a에서 비싸게 배운 것):**
1. sRGB를 끈 마스크류 텍스처는 **반드시** `TC_MASKS`로 임포트하고 머티리얼에서 `SAMPLERTYPE_MASKS`로 샘플링한다. `TC_Default`로 두면 "Sampler type is Linear Grayscale, should be Linear Color"로 머티리얼 컴파일이 **조용히** 실패하고 해당 오브젝트가 회색 기본 머티리얼로 그려진다.
2. 캡처를 만든 뒤에는 **반드시 Read 도구로 직접 본다.** 스크립트가 `_OK`를 냈다는 것과 화면이 바뀌었다는 것은 별개다.

---

### Task 0: 기준 보관

- [ ] **Step 1: 브랜치 확인**

```bash
cd /Users/hans/dev/aquarium
git status --short && git rev-parse --abbrev-ref HEAD
```
기대: 출력이 비어 있고 브랜치가 `feat/m4b-reef-lighting`.

- [ ] **Step 2: 개선 전 산출물 보관**

```bash
B=/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad/m4b_before
mkdir -p "$B"
cp docs/reviews/2026-09-21-m4a-scene.png "$B/before-scene.png"
cp assets/blender/export/preview_corals.png "$B/before-corals.png"
ls -l "$B"
```

- [ ] **Step 3: 개선 전 산호 미리보기를 Read 도구로 본다** — 흰 덩어리로 보이는 현재 상태를 눈으로 기록해 둔다(`$B/before-corals.png`).

---

### Task 1: `fishlib`에 융기 패턴 두 개

**Files:** Modify `assets/blender/fishlib.py`

- [ ] **Step 1: `ridge_pattern` 추가** — 파일 끝에 붙인다.

```python
def ridge_pattern(nt, axis, period, sharpness=1.0):
    """Banded ridges repeating every `period` cm along one world axis.

    `axis` is 'X', 'Y' or 'Z'. Returns (height_socket, roughness_socket) in 0..1, exactly the
    same contract as scale_pattern_world: feed the height into a Bump node and the roughness
    into the Principled Roughness input. Ridge crests are smooth and the troughs are wider than
    the crests (sharpness > 1 widens the troughs further), which is what a banded coral or a
    ringed tube reads as. World position is used, so `period` really is centimetres."""
    sep = position_axis(nt)
    scale = nt.nodes.new("ShaderNodeMath"); scale.operation = 'MULTIPLY'
    scale.inputs[1].default_value = 1.0 / max(1e-6, period)
    nt.links.new(sep.outputs[axis], scale.inputs[0])
    wave = nt.nodes.new("ShaderNodeMath"); wave.operation = 'SINE'
    tau = nt.nodes.new("ShaderNodeMath"); tau.operation = 'MULTIPLY'
    tau.inputs[1].default_value = 6.283185307179586
    nt.links.new(scale.outputs[0], tau.inputs[0])
    nt.links.new(tau.outputs[0], wave.inputs[0])
    norm = nt.nodes.new("ShaderNodeMapRange")          # -1..1 -> 0..1
    norm.inputs["From Min"].default_value = -1.0
    norm.inputs["From Max"].default_value = 1.0
    norm.clamp = True
    nt.links.new(wave.outputs[0], norm.inputs["Value"])
    sharp = nt.nodes.new("ShaderNodeMath"); sharp.operation = 'POWER'
    sharp.inputs[1].default_value = max(0.05, sharpness)
    nt.links.new(norm.outputs[0], sharp.inputs[0])
    rough = nt.nodes.new("ShaderNodeMapRange")         # crests slicker, troughs duller
    rough.inputs["To Min"].default_value = 0.75
    rough.inputs["To Max"].default_value = 0.45
    rough.clamp = True
    nt.links.new(sharp.outputs[0], rough.inputs["Value"])
    return sharp.outputs[0], rough.outputs[0]


def radial_ridge_pattern(nt, period, sharpness=1.0):
    """ridge_pattern driven by the XY radius from the object origin instead of one axis, so the
    ridges form concentric rings. Plate corals grow in rings; a banded pattern along X on a disc
    reads as corrugated cardboard instead. Same (height_socket, roughness_socket) contract."""
    sep = position_axis(nt)
    sx = nt.nodes.new("ShaderNodeMath"); sx.operation = 'MULTIPLY'
    nt.links.new(sep.outputs["X"], sx.inputs[0]); nt.links.new(sep.outputs["X"], sx.inputs[1])
    sy = nt.nodes.new("ShaderNodeMath"); sy.operation = 'MULTIPLY'
    nt.links.new(sep.outputs["Y"], sy.inputs[0]); nt.links.new(sep.outputs["Y"], sy.inputs[1])
    add = nt.nodes.new("ShaderNodeMath"); add.operation = 'ADD'
    nt.links.new(sx.outputs[0], add.inputs[0]); nt.links.new(sy.outputs[0], add.inputs[1])
    radius = nt.nodes.new("ShaderNodeMath"); radius.operation = 'SQRT'
    nt.links.new(add.outputs[0], radius.inputs[0])
    scale = nt.nodes.new("ShaderNodeMath"); scale.operation = 'MULTIPLY'
    scale.inputs[1].default_value = 6.283185307179586 / max(1e-6, period)
    nt.links.new(radius.outputs[0], scale.inputs[0])
    wave = nt.nodes.new("ShaderNodeMath"); wave.operation = 'SINE'
    nt.links.new(scale.outputs[0], wave.inputs[0])
    norm = nt.nodes.new("ShaderNodeMapRange")
    norm.inputs["From Min"].default_value = -1.0
    norm.inputs["From Max"].default_value = 1.0
    norm.clamp = True
    nt.links.new(wave.outputs[0], norm.inputs["Value"])
    sharp = nt.nodes.new("ShaderNodeMath"); sharp.operation = 'POWER'
    sharp.inputs[1].default_value = max(0.05, sharpness)
    nt.links.new(norm.outputs[0], sharp.inputs[0])
    rough = nt.nodes.new("ShaderNodeMapRange")
    rough.inputs["To Min"].default_value = 0.75
    rough.inputs["To Max"].default_value = 0.45
    rough.clamp = True
    nt.links.new(sharp.outputs[0], rough.inputs["Value"])
    return sharp.outputs[0], rough.outputs[0]
```

- [ ] **Step 2: 문법 확인**

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b --python-expr "import sys; sys.path.insert(0, '$PWD/assets/blender'); import fishlib; print('FISHLIB_OK', hasattr(fishlib, 'ridge_pattern'), hasattr(fishlib, 'radial_ridge_pattern'))" 2>&1 | grep FISHLIB_OK
```
기대: `FISHLIB_OK True True`

- [ ] **Step 3: 커밋**

```bash
git add assets/blender/fishlib.py
git commit -m "feat: 융기·동심 융기 패턴 헬퍼를 fishlib에 추가"
```

---

### Task 2: 산호 2종 추가 (부채·관)

**Files:** Modify `assets/blender/make_corals.py`

- [ ] **Step 1: `build_fan_coral` 추가** — `build_brain_coral` 아래에 넣는다.

```python
FAN_HEIGHT = 60.0
TUBE_HEIGHT = 45.0


def build_fan_coral(name, seed=4711, height=FAN_HEIGHT, thickness=1.6, depth=4,
                    root_len=18.0, root_radius=2.4):
    """Sea fan: the branch-coral recursion constrained to the XZ plane, so the whole colony is a
    flat lattice one or two centimetres thick. Children fan out to either side of the parent with
    a strong +Z bias; because every direction has y = 0 the silhouette is a fan rather than a
    bush. Joined, scaled to `height`, then thickened along Y by Solidify.
    Precondition: object mode, scene reset. Returns the joined object."""
    rng = random.Random(seed)
    segments = []

    def add_segment(start, direction, length, radius):
        bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=radius, radius2=radius * 0.7,
                                        depth=length, location=(0, 0, 0))
        seg = bpy.context.object
        seg.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler()
        seg.location = start + direction * (length / 2)
        segments.append(seg)
        return start + direction * length, radius * 0.7

    def grow(start, direction, length, radius, level):
        tip, tip_radius = add_segment(start, direction, length, radius)
        if level >= depth:
            return
        for sign in (-1.0, 1.0):
            tilt = rng.uniform(0.35, 0.70) * sign
            child = (Matrix.Rotation(tilt, 3, Vector((0, 1, 0))) @ direction)
            child = (child + Vector((0, 0, 0.35))).normalized()
            child.y = 0.0
            child.normalize()
            grow(tip, child, length * rng.uniform(0.62, 0.80), tip_radius, level + 1)

    grow(Vector((0, 0, 0)), Vector((0, 0, 1)), root_len, root_radius, 0)

    bpy.ops.object.select_all(action='DESELECT')
    for s in segments:
        s.select_set(True)
    ob = segments[0]
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.join()
    ob.name = name
    ob.data.name = name
    ob.scale = (height / ob.dimensions.z,) * 3
    bpy.ops.object.transform_apply(scale=True)
    sol = ob.modifiers.new("Solidify", 'SOLIDIFY')
    sol.thickness = thickness
    sol.offset = 0
    bpy.ops.object.modifier_apply(modifier="Solidify")
    bpy.ops.object.shade_smooth()
    return ob
```

- [ ] **Step 2: `build_tube_coral` 추가** — 바로 아래에 넣는다.

```python
def build_tube_coral(name, seed=9182, count=11, height=TUBE_HEIGHT, radius=4.2,
                     spread=16.0, sides=10):
    """Cluster of upright open-topped tubes rising from a common base. Each tube leans slightly
    outward from the cluster centre, has its own height (x0.55..1.0) and its own radius
    (x0.75..1.15), and is hollow at the top: the rim is a ring of two concentric circles, which
    is what makes it read as a tube rather than a peg. Joined into one mesh.
    Precondition: object mode, scene reset. Returns the joined object."""
    rng = random.Random(seed)
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()

    for i in range(count):
        angle = 2 * math.pi * i / count + rng.uniform(-0.25, 0.25)
        dist = spread * math.sqrt(rng.uniform(0.0, 1.0))
        bx, by = dist * math.cos(angle), dist * math.sin(angle)
        h = height * rng.uniform(0.55, 1.0)
        r_out = radius * rng.uniform(0.75, 1.15)
        r_in = r_out * 0.62
        lean_x = (bx / max(1e-6, spread)) * h * 0.14
        lean_y = (by / max(1e-6, spread)) * h * 0.14

        def ring(z, r, ox, oy):
            return [bm.verts.new((bx + ox + r * math.cos(2 * math.pi * j / sides),
                                  by + oy + r * math.sin(2 * math.pi * j / sides), z))
                    for j in range(sides)]

        bottom = ring(0.0, r_out, 0.0, 0.0)
        top_out = ring(h, r_out * 0.88, lean_x, lean_y)
        top_in = ring(h, r_in * 0.88, lean_x, lean_y)
        inner_bottom = ring(h * 0.25, r_in, lean_x * 0.25, lean_y * 0.25)
        for j in range(sides):
            k = (j + 1) % sides
            bm.faces.new((bottom[j], top_out[j], top_out[k], bottom[k]))          # outer wall
            bm.faces.new((top_out[j], top_in[j], top_in[k], top_out[k]))          # rim
            bm.faces.new((top_in[k], inner_bottom[k], inner_bottom[j], top_in[j]))  # inner wall
        bm.faces.new(list(reversed(inner_bottom)))                                 # tube floor
        bm.faces.new(bottom)                                                       # base cap

    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()

    ob = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.shade_smooth()
    return ob
```

- [ ] **Step 3: `SPECS`에 두 종 추가하고 미리보기 간격을 5개로** — `SPECS` 리스트에 아래 두 항목을 덧붙이고 `render_group_preview`의 기본 `xs`를 다섯 자리로 바꾼다.

```python
    dict(name="FanCoral", build=build_fan_coral,
         color=coral_color_fn((0.92, 0.45, 0.30, 1), (0.55, 0.20, 0.14, 1), scale=13.0),
         dims=((20, 90), (1, 12), (58, 62))),
    dict(name="TubeCoral", build=build_tube_coral,
         color=coral_color_fn((0.55, 0.80, 0.72, 1), (0.25, 0.45, 0.42, 1), scale=8.0),
         dims=((25, 60), (25, 60), (24, 48))),
```

```python
def render_group_preview(specs, path, xs=(-260.0, -130.0, 0.0, 130.0, 260.0)):
```
그리고 `render_group_preview` 호출부의 카메라를 다섯 개가 다 들어오게 뒤로 뺀다:
```python
    F.render_preview(path, cam_loc=(0.0, -900.0, 150.0), cam_rot=(1.40, 0.0, 0.0))
```
또 `check_export_ready`의 기본 정점 상한을 늘린다(부채·관 산호가 더 많다):
```python
def check_export_ready(ob, verts=(200, 14000), dims=None):
```

- [ ] **Step 4: 실행과 확인**

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_corals.py 2>&1 | tail -20
```
기대: `CORALS_OK branch=<n> plate=<n> brain=<n> fan=<n> tube=<n> dims=[...]`
(마지막 `print`를 5종으로 고칠 것:)
```python
    print("CORALS_OK " + " ".join(
        "%s=%d" % (s["name"], r["verts"]) for s, r in zip(SPECS, results))
        + " dims=%s" % [r["dims"] for r in results])
```

- [ ] **Step 5: 미리보기를 Read 도구로 본다** — `assets/blender/export/preview_corals.png`에서 부채 산호가 납작한 부채로, 관 산호가 열린 관 다발로 보이는지 확인한다. 아니면 Step 1/2의 수치를 고쳐 최대 3회 반복한다.

- [ ] **Step 6: 커밋**

```bash
git add assets/blender/make_corals.py assets/blender/export assets/blender/*.blend
git commit -m "feat: 부채 산호·관 산호 절차 생성 추가 (산호 5종)"
```

---

### Task 3: 산호 PBR 3장 베이크

**Files:** Modify `assets/blender/make_corals.py`

- [ ] **Step 1: `coral_maps_fn` 추가** — 기존 `coral_color_fn` 아래에 넣는다(`coral_color_fn`은 미리보기에서 계속 쓰므로 남긴다).

```python
def coral_maps_fn(base, shade, detail, scale=7.0, bump_strength=0.35):
    """Return a build_nodes(nt, bsdf, ctx) callback for fishlib.bake_maps.

    `base`/`shade` are the two tints mixed by a contrast-stretched noise texture, exactly as
    coral_color_fn does, so the baked base colour is unchanged in character. `detail(nt, ctx)`
    returns (height_socket, roughness_socket): the height drives a Bump node into the Principled
    Normal input (that is what the NORMAL bake picks up) and the roughness goes straight into
    Roughness. The height also darkens the base colour in the troughs, because a groove that is
    only a normal-map dent still reads flat under flat ambient light."""
    def build(nt, bsdf, ctx):
        tex = nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = scale
        tex.inputs["Detail"].default_value = 6.0
        tex.inputs["Roughness"].default_value = 0.55
        stretch = nt.nodes.new("ShaderNodeMapRange")
        stretch.inputs["From Min"].default_value = 0.35
        stretch.inputs["From Max"].default_value = 0.65
        nt.links.new(tex.outputs["Fac"], stretch.inputs["Value"])
        col = F.mix_over(nt, base, stretch.outputs[0], shade)

        height, rough = detail(nt, ctx)

        # troughs (height near 0) get the shade tint laid over the mottled base
        ao = nt.nodes.new("ShaderNodeMath"); ao.operation = 'SUBTRACT'
        ao.inputs[0].default_value = 1.0
        nt.links.new(height, ao.inputs[1])
        darkened = F.mix_over(nt, col, ao.outputs[0], shade)
        nt.links.new(darkened, bsdf.inputs["Base Color"])

        nt.links.new(rough, bsdf.inputs["Roughness"])

        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = bump_strength
        nt.links.new(height, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return build
```

- [ ] **Step 2: 종별 표면 디테일 다섯 개** — `coral_maps_fn` 아래에 넣는다.

```python
def detail_branch(nt, ctx):
    """Branch coral: small polyp bumps all over the tapered cylinders."""
    return ctx["scale_pattern_world"](nt, cell_size=1.1, sharpness=0.9)


def detail_plate(nt, ctx):
    """Plate coral: concentric growth rings, 2.4 cm apart, plus a fine polyp break-up."""
    return F.radial_ridge_pattern(nt, period=2.4, sharpness=1.3)


def detail_brain(nt, ctx):
    """Brain coral: a second, much finer meander on top of the geometric grooves. The mesh
    already carries the big convolutions; what is missing at 10 m is the texture inside them."""
    return ctx["scale_pattern_world"](nt, cell_size=0.8, sharpness=1.6)


def detail_fan(nt, ctx):
    """Sea fan: fine cross-hatch along the lattice, read as the polyp rows on each branch."""
    return F.ridge_pattern(nt, axis="Z", period=0.9, sharpness=1.1)


def detail_tube(nt, ctx):
    """Tube coral: horizontal growth bands around each tube, 1.8 cm apart."""
    return F.ridge_pattern(nt, axis="Z", period=1.8, sharpness=1.4)
```

- [ ] **Step 3: `bake_maps`가 `scale_pattern_world`를 ctx로 넘기게 한다** — `assets/blender/fishlib.py`의 `bake_maps` 안 `ctx` 딕셔너리에 한 줄 추가한다.

```python
    ctx = {"scale_pattern": scale_pattern, "scale_pattern_world": scale_pattern_world,
           "position_axis": position_axis, "axis_band_mask": axis_band_mask,
           "mix_over": mix_over, "ridge_pattern": ridge_pattern,
           "radial_ridge_pattern": radial_ridge_pattern}
```

- [ ] **Step 4: `SPECS`에 `maps` 키를 넣고 `build_coral`을 `bake_maps`로 바꾼다**

각 spec에 `maps=coral_maps_fn(<base>, <shade>, <detail>, scale=<기존 scale>)`을 추가한다
(`color`는 미리보기용으로 남긴다). 종별 detail 배정: BranchCoral → `detail_branch`,
PlateCoral → `detail_plate`, BrainCoral → `detail_brain`, FanCoral → `detail_fan`,
TubeCoral → `detail_tube`.

```python
def build_coral(spec):
    """Full pipeline for one coral: fresh scene -> geometry -> unwrap -> bake three maps ->
    guards -> .blend + .fbx. Returns dict(verts, dims)."""
    name = spec["name"]
    F.reset_scene()
    ob = spec["build"](name)
    F.unwrap(ob)
    F.bake_maps(ob, "M_" + name, spec["maps"], name, EXPORT, size=TEXTURE_SIZE)
    check_export_ready(ob, dims=spec["dims"])
    save_and_export_static(ob, os.path.join(ROOT, name + ".blend"),
                           os.path.join(EXPORT, name + ".fbx"))
    return dict(verts=len(ob.data.vertices),
                dims=tuple(round(v, 2) for v in ob.dimensions))
```
`TEXTURE_SIZE`는 1024로 유지한다(산호는 화면에서 물고기보다 작다).

- [ ] **Step 5: 실행**

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_corals.py 2>&1 | tail -20
ls -l assets/blender/export/T_*Coral_*.png | wc -l
```
기대: `CORALS_OK ...` 한 줄, 그리고 텍스처 파일 수가 `15`(5종 × 3장). 베이크가 평평하면
`bake_maps`의 `_assert_not_degenerate`가 `RuntimeError`로 멈추므로, 그 경우
`coral_maps_fn`의 `bump_strength`나 detail의 `sharpness`를 올려 다시 돌린다.

- [ ] **Step 6: 미리보기를 Read 도구로 본다** — `assets/blender/export/preview_corals.png`.
(미리보기는 `color`를 쓰므로 노멀은 안 보인다. 노멀맵 자체는 다음 명령으로 확인한다.)

```bash
for n in BranchCoral PlateCoral BrainCoral FanCoral TubeCoral; do
  /Applications/Blender.app/Contents/MacOS/Blender -b --python-expr "
import bpy, sys
sys.path.insert(0, '$PWD/assets/blender')
img = bpy.data.images.load('$PWD/assets/blender/export/T_${n}_Normal.png')
px = img.pixels[:]; n_px = len(px)//4; step = max(1, n_px//20000)
vals = [px[i*4+1] for i in range(0, n_px, step)]
m = sum(vals)/len(vals)
print('NORMAL_OK ${n}', img.size[0], round(sum((v-m)**2 for v in vals)/len(vals), 6))
" 2>&1 | grep NORMAL_OK
done
```
기대: 다섯 줄 모두 `1024`이고 분산이 `0.0`이 아니다.

- [ ] **Step 7: 커밋**

```bash
git add assets/blender
git commit -m "feat: 산호 5종을 BaseColor·Normal·Roughness 3장 베이크로 전환"
```

---

### Task 4: Unreal 산호 PBR 머티리얼과 틴트 인스턴스

**Files:** Modify `unreal/Aquarium/Scripts/import_props.py`

- [ ] **Step 1: 상수 갱신**

```python
CORALS = ["BranchCoral", "PlateCoral", "BrainCoral", "FanCoral", "TubeCoral"]
ROCKS = ["boulder_01", "rock_07", "rock_09"]
ROCK_SCALE = 1.0

# Per-instance colour variation: one MaterialInstanceConstant per tint, assigned to
# individual prop actors by build_reef_m1.py. Three tints per coral (the reef should not
# look stamped), two per rock (the rocks already vary by mesh and by AO).
CORAL_TINTS = [(1.00, 1.00, 1.00), (0.82, 0.90, 1.05), (1.10, 0.92, 0.86)]
ROCK_TINTS = [(1.00, 1.00, 1.00), (0.90, 0.95, 1.06)]
```

- [ ] **Step 2: 틴트 파라미터와 인스턴스 헬퍼** — `assign_material` 아래에 넣는다.

```python
def add_tint(mat, source, source_output="", x=-150, y=-100):
    """Multiply `source`'s output by a 'Tint' vector parameter and return the multiply node, so
    the caller can connect it to Base Color. `source_output` is the output name on `source`
    ("RGB" for a TextureSample, "" for an arithmetic node) -- passed explicitly rather than
    sniffed from the node class, because a wrong guess here connects nothing and the tint simply
    never appears. The parameter is what the per-instance MaterialInstanceConstants below
    override; without it every copy of a mesh is the same colour and 22 props read as stamped."""
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y + 200)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(r=1.0, g=1.0, b=1.0, a=1.0))
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x + 150, y)
    mel.connect_material_expressions(source, source_output, mul, "A")
    mel.connect_material_expressions(tint, "RGB", mul, "B")
    return mul


def make_tint_instances(mat, name, tints):
    """Create (or reuse) MI_<name>_v0..vN as MaterialInstanceConstants of `mat`, each with its
    own Tint. Reused in place so re-running never leaves MI_*_1 duplicates behind. Returns the
    list of instances."""
    out = []
    for i, (r, g, b) in enumerate(tints):
        path = DEST + "/MI_%s_v%d" % (name, i)
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
            assert isinstance(mi, unreal.MaterialInstanceConstant), "not a MIC: %s" % path
        else:
            mi = asset_tools.create_asset("MI_%s_v%d" % (name, i), DEST,
                                          unreal.MaterialInstanceConstant,
                                          unreal.MaterialInstanceConstantFactoryNew())
        assert mi is not None, "material instance creation failed: %s" % path
        mel.set_material_instance_parent(mi, mat)
        mel.set_material_instance_vector_parameter_value(
            mi, "Tint", unreal.LinearColor(r=r, g=g, b=b, a=1.0))
        eal.save_loaded_asset(mi)
        out.append(mi)
    return out
```

- [ ] **Step 3: `import_coral`을 3장 PBR + 틴트로 다시 쓴다**

```python
def import_coral(name):
    src = os.path.join(ROOT, "assets", "blender", "export")
    base = import_texture(os.path.join(src, "T_%s_BaseColor.png" % name),
                          "T_%s_BaseColor" % name, srgb=True)
    nor = import_texture(os.path.join(src, "T_%s_Normal.png" % name),
                         "T_%s_Normal" % name, srgb=False,
                         compression=unreal.TextureCompressionSettings.TC_NORMALMAP)
    # TC_MASKS, not just srgb=False -- see the note in import_rock(); a roughness map left on
    # TC_Default fails the whole material compile and the coral silently turns grey.
    rgh = import_texture(os.path.join(src, "T_%s_Roughness.png" % name),
                         "T_%s_Roughness" % name, srgb=False,
                         compression=unreal.TextureCompressionSettings.TC_MASKS)
    sm = import_static_mesh(os.path.join(src, "%s.fbx" % name), "SM_%s" % name, 1.0)

    mat = get_or_create_material("M_%s" % name)
    n_base = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, -200)
    n_base.set_editor_property("texture", base)
    mel.connect_material_property(add_tint(mat, n_base, "RGB"), "",
                                  unreal.MaterialProperty.MP_BASE_COLOR)

    n_nor = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 200)
    n_nor.set_editor_property("texture", nor)
    n_nor.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(n_nor, "RGB", unreal.MaterialProperty.MP_NORMAL)

    n_rgh = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 500)
    n_rgh.set_editor_property("texture", rgh)
    n_rgh.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_property(n_rgh, "R", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    instances = make_tint_instances(mat, name, CORAL_TINTS)

    assign_material(sm, instances[0])
    return [sm], [mat], [base, nor, rgh], instances
```

- [ ] **Step 4: `import_rock`에 틴트 추가** — 베이스 컬러 연결부만 바꾼다.

```python
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -100)
    mel.connect_material_expressions(n_diff, "RGB", mul, "A")
    mel.connect_material_expressions(n_ao, "RGB", mul, "B")
    mel.connect_material_property(add_tint(mat, mul, ""), "", unreal.MaterialProperty.MP_BASE_COLOR)
```
그리고 `mel.recompile_material(mat)` 뒤에:
```python
    instances = make_tint_instances(mat, rid, ROCK_TINTS)
    assign_material(sm, instances[0])
    return [sm], [mat], [diff, nor, rgh, ao], instances
```

- [ ] **Step 5: 수집 루프와 출력** — 파일 끝을 바꾼다.

```python
meshes, materials, textures, instances = [], [], [], []
for n in CORALS:
    m, mt, tx, mi = import_coral(n)
    meshes += m; materials += mt; textures += tx; instances += mi
for r in ROCKS:
    m, mt, tx, mi = import_rock(r)
    meshes += m; materials += mt; textures += tx; instances += mi

print("PROPS_OK count=%d meshes=%d materials=%d instances=%d textures=%d assets=[%s]" % (
    len(meshes) + len(materials) + len(instances) + len(textures), len(meshes),
    len(materials), len(instances), len(textures),
    ", ".join(str(diag(sm)) for sm in meshes)))
print("PROPS_OK materials=[%s]" % ", ".join(m.get_path_name() for m in materials))
print("PROPS_OK instances=[%s]" % ", ".join(m.get_path_name() for m in instances))
print("PROPS_OK textures=[%s]" % ", ".join(t.get_path_name() for t in textures))
```

- [ ] **Step 6: 실행(두 번 — 멱등성 확인)**

```bash
UE="/Users/Shared/Epic Games/UE_5.8"
for pass in 1 2; do
  "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" \
    -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/import_props.py" \
    -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 \
    | grep -E "PROPS_OK|Traceback|LogPython: Error|Failed to compile Material"
done
```
기대: 두 번 다 `PROPS_OK count=... meshes=8 materials=8 instances=21 textures=27`,
`Failed to compile Material` 0회, 경로에 `_1` 접미사가 붙은 에셋 없음.

- [ ] **Step 7: 커밋**

```bash
git add unreal/Aquarium/Scripts/import_props.py unreal/Aquarium/Content/Props
git commit -m "feat: 산호 PBR 머티리얼 3장 연결과 프롭 틴트 머티리얼 인스턴스"
```

---

### Task 5: 조명 — 보이지 않는 수면 차폐물과 안개 재튜닝

**Files:** Modify `unreal/Aquarium/Scripts/build_reef_m1.py`, `unreal/Aquarium/Config/DefaultEngine.ini`

- [ ] **Step 1: 튜닝 상수 교체** — 기존 값을 아래로 바꾼다.

```python
SUN_VOLUMETRIC_SCATTERING = 14.0        # was 8.0: shafts need scattering AND an occluder

# M4b: the M4a capture lost every colour by ~10 m, which is why the 36-fish school read as
# flat silhouettes. Density 2.2 with extinction 0.7 keeps species colour to about 20 m while
# still fogging the 40 m floor edge away. start_distance clears the nearest 1.5 m so the
# player's own fish stays crisp.
FOG_DENSITY = 2.2
FOG_HEIGHT_FALLOFF = 0.0
FOG_START_DISTANCE = 150.0
FOG_INSCATTER = dict(r=0.015, g=0.105, b=0.175)         # darker: far = deep water, not bright sky
FOG_DIRECTIONAL_INSCATTER = dict(r=0.12, g=0.38, b=0.48)
FOG_DIRECTIONAL_EXPONENT = 6.0                          # wider sun glow
FOG_ALBEDO = dict(r=30, g=140, b=200, a=255)
FOG_SCATTERING_DISTRIBUTION = 0.6
FOG_EXTINCTION_SCALE = 0.7
FOG_VOLUMETRIC_DISTANCE = 6000.0

# Surface gobo: an invisible shadow caster at the waterline. Volumetric god rays are
# scattering intensity TIMES shadow contrast, and this scene had nothing above the camera to
# cast a shadow at all -- which is why "빛줄기 약함" survived every intensity increase in M1.
GOBO_Z = 1100.0
GOBO_SCALE = 60.0                       # 100 cm plane -> 60 m, wider than the 40 m floor
GOBO_THRESHOLD = 0.42                   # opacity-mask cutoff; higher = narrower, sharper shafts
```

- [ ] **Step 2: `M_SurfaceGobo` 빌더** — `build_caustics` 아래에 넣는다.

```python
def build_surface_gobo(m):
    """Opacity-mask material for the invisible waterline plane. Reuses the caustics wave sum:
    where the sum is above GOBO_THRESHOLD the plane is solid and blocks the sun, elsewhere it is
    cut away and the sun gets through. The holes therefore drift with the same rhythm as the
    floor caustics, so the shafts and the floor patches move together. Unlit and masked; the
    plane is never drawn (bCastHiddenShadow), so its colour does not matter."""
    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)

    def const(x, y, v):
        n = node(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property("r", v)
        return n

    def mask(src, x, y, r, g):
        n = node(unreal.MaterialExpressionComponentMask, x, y)
        n.set_editor_property("r", r); n.set_editor_property("g", g)
        n.set_editor_property("b", False); n.set_editor_property("a", False)
        mel.connect_material_expressions(src, "", n, "")
        return n

    def mul(a, b, x, y):
        n = node(unreal.MaterialExpressionMultiply, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        return n

    def add(a, b, x, y):
        n = node(unreal.MaterialExpressionAdd, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        return n

    def sine(src, x, y):
        n = node(unreal.MaterialExpressionSine, x, y)
        mel.connect_material_expressions(src, "", n, "")
        return n

    wpos = node(unreal.MaterialExpressionWorldPosition, -2000, 0)
    px = mask(wpos, -1800, -100, True, False)
    py = mask(wpos, -1800, 100, False, True)
    t = node(unreal.MaterialExpressionTime, -1800, 300)
    waves = []
    for i, (period, angle, drift) in enumerate(CAUSTIC_WAVES[:3]):
        y = -450 + i * 220
        kx = math.cos(math.radians(angle)) / (period * 3.0)   # 3x coarser than the floor pattern
        ky = math.sin(math.radians(angle)) / (period * 3.0)
        ax = mul(px, const(-1600, y - 60, kx), -1450, y - 60)
        ay = mul(py, const(-1600, y, ky), -1450, y)
        at = mul(t, const(-1600, y + 60, drift * 0.5), -1450, y + 60)
        waves.append(sine(add(add(ax, ay, -1300, y), at, -1150, y), -700, y))
    s = waves[0]
    for i, w in enumerate(waves[1:]):
        s = add(s, w, -550 + i * 100, -100)
    n_waves = float(len(waves))
    norm = add(mul(s, const(-250, 100, 1.0 / (2.0 * n_waves)), -150, -100),
               const(-150, 100, 0.5), -50, -100)
    # opacity mask = saturate((norm - threshold) * 8): a hard-ish edge so the shafts have edges
    off = node(unreal.MaterialExpressionSubtract, 50, -100)
    mel.connect_material_expressions(norm, "", off, "A")
    mel.connect_material_expressions(const(50, 100, GOBO_THRESHOLD), "", off, "B")
    gain = mul(off, const(200, 100, 8.0), 300, -100)
    sat = node(unreal.MaterialExpressionSaturate, 450, -100)
    mel.connect_material_expressions(gain, "", sat, "")
    mel.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(const(450, 200, 0.0), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)


m_gobo = material("M_SurfaceGobo")
m_gobo.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
m_gobo.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m_gobo.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m_gobo.set_editor_property("two_sided", True)
build_surface_gobo(m_gobo)
mel.recompile_material(m_gobo)
eal.save_loaded_asset(m_gobo)
```
이 블록은 `m_caus` 블록 바로 다음, `# --- level ---` 앞에 둔다.

- [ ] **Step 3: 안개 액터 설정 갱신** — 기존 `fog` 블록에 두 줄을 더한다.

```python
fc.set_editor_property("start_distance", FOG_START_DISTANCE)
fc.set_editor_property("directional_inscattering_exponent", FOG_DIRECTIONAL_EXPONENT)
```
(그리고 `fog_density`, `volumetric_fog_extinction_scale`, 인스캐터 색은 Step 1의 새 상수를 그대로 쓴다.)

- [ ] **Step 4: 차폐물 액터 스폰** — `fog` 블록 다음, `cam` 블록 앞에 넣는다.

```python
# Invisible shadow caster: set_visibility(False) + cast_hidden_shadow is Unreal's supported way
# to have geometry that is never drawn but still occludes light, so nothing appears in the sky
# above the diver while the volumetric fog gains the shadow contrast it needs for shafts.
gobo = spawn(unreal.StaticMeshActor, (400, 0, GOBO_Z), label="SurfaceGobo")
gc = gobo.static_mesh_component
gc.set_mobility(unreal.ComponentMobility.STATIC)
assert gc.set_static_mesh(unreal.load_asset(ENGINE_PLANE)), "gobo mesh missing: %s" % ENGINE_PLANE
gc.set_material(0, m_gobo)
gobo.set_actor_scale3d(unreal.Vector(GOBO_SCALE, GOBO_SCALE, 1))
gc.set_visibility(False)
gc.set_cast_hidden_shadow(True)
gc.set_editor_property("cast_shadow", True)
```

- [ ] **Step 5: 볼류메트릭 격자 품질** — `unreal/Aquarium/Config/DefaultEngine.ini`의
`[/Script/Engine.RendererSettings]` 섹션에 두 줄을 더한다.

```ini
; M4b: default 8/64 makes the god-ray edges stair-step. These two are the first entries on the
; M4b cut list if the frame budget breaks.
r.VolumetricFog.GridPixelSize=4
r.VolumetricFog.GridSizeZ=128
```

- [ ] **Step 6: 실행**

```bash
UE="/Users/Shared/Epic Games/UE_5.8"
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" \
  -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/build_reef_m1.py" \
  -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 \
  | grep -E "REEF_OK|REEF_WARN|Traceback|LogPython: Error|Failed to compile Material"
```
기대: `REEF_OK actors=<n> fish=36 props=14 map=/Game/Maps/ReefM1`(프롭 수는 Task 8에서 늘린다),
`Failed to compile Material` 0회.

- [ ] **Step 7: 커밋**

```bash
git add unreal/Aquarium/Scripts/build_reef_m1.py unreal/Aquarium/Config/DefaultEngine.ini unreal/Aquarium/Content
git commit -m "feat: 보이지 않는 수면 차폐물로 빛줄기 생성, 안개 밀도 재튜닝"
```

---

### Task 6: 후처리 볼륨 (색보정·블룸·AO, 심도는 명시적으로 끔)

**Files:** Modify `unreal/Aquarium/Scripts/build_reef_m1.py`

- [ ] **Step 1: 상수 추가** — `GOBO_*` 아래에 넣는다.

```python
# Post-process. Auto exposure is already off project-wide (r.DefaultFeature.AutoExposure=False),
# so these are absolute. Lowering the fog costs contrast; the grade puts it back.
PP_SATURATION = 1.15
PP_CONTRAST = 1.06
PP_TEMPERATURE = 5200.0                 # slightly warm, to offset the all-over blue cast
PP_BLOOM_INTENSITY = 0.5
PP_BLOOM_THRESHOLD = 1.0
PP_AO_INTENSITY = 0.5
PP_AO_RADIUS = 80.0

# Depth of field is OFF BY DEFAULT and explicitly overridden off, so a change to the project
# renderer settings cannot quietly turn it on. The camera is fixed and the thing the child is
# looking at -- their own fish -- sits close to the camera, so DoF risks blurring exactly the
# subject, and it costs frame time. To try it, set DOF to e.g.
# dict(fstop=2.8, focal_distance=220.0) and re-run this script; nothing else changes.
DOF = None
```

- [ ] **Step 2: 볼륨 스폰** — `cam` 블록 다음, 물고기 루프 앞에 넣는다.

```python
pp = spawn(unreal.PostProcessVolume, (0, 0, 200), label="Grade")
pp.set_editor_property("unbound", True)
pp.set_editor_property("priority", 1.0)
pp.set_editor_property("blend_weight", 1.0)
s = pp.get_editor_property("settings")


def pp_set(name, value):
    """Set one post-process property and its override_ flag. A PostProcessSettings field with
    its override flag left False is simply ignored, which is the classic way a grade 'does
    nothing' while looking correct in the details panel."""
    s.set_editor_property("override_" + name, True)
    s.set_editor_property(name, value)


pp_set("color_saturation", unreal.Vector4(PP_SATURATION, PP_SATURATION, PP_SATURATION, 1.0))
pp_set("color_contrast", unreal.Vector4(PP_CONTRAST, PP_CONTRAST, PP_CONTRAST, 1.0))
pp_set("white_temp", PP_TEMPERATURE)
pp_set("bloom_intensity", PP_BLOOM_INTENSITY)
pp_set("bloom_threshold", PP_BLOOM_THRESHOLD)
pp_set("ambient_occlusion_intensity", PP_AO_INTENSITY)
pp_set("ambient_occlusion_radius", PP_AO_RADIUS)
if DOF is None:
    # fstop 32 + focal distance 0 = no circle of confusion anywhere in the frame
    pp_set("depth_of_field_fstop", 32.0)
    pp_set("depth_of_field_focal_distance", 0.0)
else:
    pp_set("depth_of_field_fstop", DOF["fstop"])
    pp_set("depth_of_field_focal_distance", DOF["focal_distance"])
pp.set_editor_property("settings", s)
```

- [ ] **Step 3: `REEF_OK` 줄에 후처리 상태를 싣는다**

```python
print("REEF_OK actors=%d fish=%d props=%d dof=%s map=%s"
      % (len(eas.get_all_level_actors()), fish_count, prop_count,
         "off" if DOF is None else "on", MAP))
```

- [ ] **Step 4: 실행** — Task 5 Step 6과 같은 명령. 기대: `REEF_OK ... dof=off ...`.

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Scripts/build_reef_m1.py unreal/Aquarium/Content
git commit -m "feat: 색보정·블룸·AO 후처리 볼륨 추가, 심도는 기본 끔"
```

---

### Task 7: 부유 입자 — 해석적 머티리얼 커튼 3장

**Files:** Modify `unreal/Aquarium/Scripts/build_reef_m1.py`

- [ ] **Step 1: 상수 추가** — `DOF` 아래에 넣는다.

```python
# Marine snow. NOT Niagara: an emitter graph cannot be built from editor Python (only an empty
# NiagaraSystem can), so a Niagara version would mean committing a hand-made .uasset, which this
# project does not do. The camera is fixed, so three translucent curtains standing across the
# view do the job with a fully scripted material graph.
# (distance X, uniform scale, cell size cm, dot radius, brightness, drift cm/s)
SNOW_CURTAINS = [
    ("near", 90.0,  3.0, 26.0, 0.11, 0.55, -7.0),
    ("mid",  240.0, 7.0, 17.0, 0.13, 0.40, -5.0),
    ("far",  480.0, 13.0, 11.0, 0.15, 0.28, -3.5),
]
SNOW_COLOR = dict(r=0.72, g=0.86, b=0.92, a=1.0)
```

- [ ] **Step 2: `M_MarineSnow` 빌더** — `build_surface_gobo` 블록 아래에 넣는다.

```python
def build_marine_snow(m):
    """Sparse drifting dots from an analytic cell hash -- no texture, no particle system.

    World Y and Z are divided into cells of CellSize cm. Each cell gets one pseudo-random
    centre from frac(sin(dot(cellId, (12.9898, 78.233))) * 43758.5453), and the pixel's
    distance to that centre becomes the dot. The whole grid slides along Z at Drift cm/s, so the
    specks sink the way marine snow does. Unlit + translucent + two-sided (two-sided removes any
    doubt about which way the plane's normal ended up pointing)."""
    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)

    def const(x, y, v):
        n = node(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property("r", v)
        return n

    def const2(x, y, a, b):
        n = node(unreal.MaterialExpressionConstant2Vector, x, y)
        n.set_editor_property("r", a)
        n.set_editor_property("g", b)
        return n

    def scalar(x, y, name, default):
        n = node(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", default)
        return n

    def mask(src, x, y, r, g, b=False):
        n = node(unreal.MaterialExpressionComponentMask, x, y)
        n.set_editor_property("r", r); n.set_editor_property("g", g)
        n.set_editor_property("b", b); n.set_editor_property("a", False)
        mel.connect_material_expressions(src, "", n, "")
        return n

    def binop(cls, a, b, x, y):
        n = node(cls, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        return n

    def unop(cls, a, x, y):
        n = node(cls, x, y)
        mel.connect_material_expressions(a, "", n, "")
        return n

    Mul, Add, Sub, Div = (unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd,
                          unreal.MaterialExpressionSubtract, unreal.MaterialExpressionDivide)
    Frac, Floor = unreal.MaterialExpressionFrac, unreal.MaterialExpressionFloor

    wpos = node(unreal.MaterialExpressionWorldPosition, -2400, 0)
    yz = mask(wpos, -2200, 0, False, True, True)          # world (Y, Z)
    cell = scalar(-2200, 250, "CellSize", 16.0)
    drift = scalar(-2200, 400, "Drift", -5.0)
    radius = scalar(-2200, 550, "DotRadius", 0.13)
    bright = scalar(-2200, 700, "Brightness", 0.4)
    t = node(unreal.MaterialExpressionTime, -2200, 850)

    # slide the field along Z: offset = (0, drift * t)
    slide = binop(Mul, drift, t, -2000, 400)
    offset = node(unreal.MaterialExpressionAppendVector, -1850, 400)
    mel.connect_material_expressions(const(-2000, 500, 0.0), "", offset, "A")
    mel.connect_material_expressions(slide, "", offset, "B")
    moved = binop(Add, yz, offset, -1700, 0)
    p = binop(Div, moved, cell, -1550, 0)                 # position in cell units
    cid = unop(Floor, p, -1400, -150)                     # cell id
    f = unop(Frac, p, -1400, 150)                         # position inside the cell

    # hash: frac(sin(dot(cid, (12.9898, 78.233))) * 43758.5453)
    dot = node(unreal.MaterialExpressionDotProduct, -1200, -150)
    mel.connect_material_expressions(cid, "", dot, "A")
    mel.connect_material_expressions(const2(-1400, -300, 12.9898, 78.233), "", dot, "B")
    h = unop(Frac, binop(Mul, unop(unreal.MaterialExpressionSine, dot, -1050, -150),
                         const(-1050, -50, 43758.5453), -900, -150), -750, -150)
    # second hash for the other axis, offset so the two are not correlated
    h2 = unop(Frac, binop(Mul, unop(unreal.MaterialExpressionSine,
                                    binop(Add, dot, const(-1050, 50, 3.7), -1050, 30),
                                    -900, 30),
                          const(-900, 130, 24634.6345), -750, 30), -600, 30)
    # clamp the centres into 0.25..0.75 so dots never straddle a cell edge
    centre = node(unreal.MaterialExpressionAppendVector, -450, -60)
    mel.connect_material_expressions(binop(Add, binop(Mul, h, const(-600, -250, 0.5), -450, -150),
                                           const(-450, -250, 0.25), -300, -150), "", centre, "A")
    mel.connect_material_expressions(binop(Add, binop(Mul, h2, const(-600, 130, 0.5), -450, 60),
                                           const(-450, 130, 0.25), -300, 60), "", centre, "B")

    d = node(unreal.MaterialExpressionDistance, -100, 0)
    mel.connect_material_expressions(f, "", d, "A")
    mel.connect_material_expressions(centre, "", d, "B")
    # dot = saturate(1 - d / radius); third hash thins the field so not every cell has a speck
    fall = unop(unreal.MaterialExpressionSaturate,
                binop(Sub, const(50, 120, 1.0), binop(Div, d, radius, 50, 0), 200, 0), 350, 0)
    thin = unop(unreal.MaterialExpressionSaturate,
                binop(Mul, binop(Sub, h, const(200, 250, 0.55), 350, 200),
                      const(350, 300, 6.0), 500, 200), 650, 200)
    opacity = binop(Mul, binop(Mul, fall, thin, 500, 0), bright, 650, 0)

    colour = node(unreal.MaterialExpressionConstant3Vector, 500, -200)
    colour.set_editor_property("constant", unreal.LinearColor(**SNOW_COLOR))
    mel.connect_material_property(binop(Mul, colour, opacity, 800, -200), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)


m_snow = material("M_MarineSnow")
m_snow.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
m_snow.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m_snow.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m_snow.set_editor_property("two_sided", True)
build_marine_snow(m_snow)
mel.recompile_material(m_snow)
eal.save_loaded_asset(m_snow)

snow_instances = {}
for tag, _x, _scale, cell, dot_r, bright, drift in SNOW_CURTAINS:
    mi_path = ENV + "/MI_MarineSnow_" + tag
    if eal.does_asset_exist(mi_path):
        mi = unreal.load_asset(mi_path)
    else:
        mi = tools.create_asset("MI_MarineSnow_" + tag, ENV,
                                unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    assert mi is not None, "snow instance creation failed: %s" % mi_path
    mel.set_material_instance_parent(mi, m_snow)
    for pname, pval in (("CellSize", cell), ("DotRadius", dot_r),
                        ("Brightness", bright), ("Drift", drift)):
        mel.set_material_instance_scalar_parameter_value(mi, pname, pval)
    eal.save_loaded_asset(mi)
    snow_instances[tag] = mi
```
이 블록은 `m_gobo` 블록 다음, `# --- level ---` 앞에 둔다.

- [ ] **Step 3: 커튼 액터 스폰** — `pp` 블록 다음에 넣는다.

```python
# The curtains stand across the fixed camera's view at three depths. They are unlit translucent
# and cast nothing, so they cannot darken the reef behind them.
for tag, x, scale, _cell, _dot_r, _bright, _drift in SNOW_CURTAINS:
    curtain = spawn(unreal.StaticMeshActor, (x, 0.0, 130.0), (90.0, 0.0),
                    label="SnowCurtain_%s" % tag)
    cc = curtain.static_mesh_component
    cc.set_mobility(unreal.ComponentMobility.STATIC)
    assert cc.set_static_mesh(unreal.load_asset(ENGINE_PLANE)), \
        "curtain mesh missing: %s" % ENGINE_PLANE
    cc.set_material(0, snow_instances[tag])
    curtain.set_actor_scale3d(unreal.Vector(scale, scale, 1))
    cc.set_editor_property("cast_shadow", False)
    cc.set_editor_property("receives_decals", False)
```

- [ ] **Step 4: 실행** — Task 5 Step 6과 같은 명령. 기대: `REEF_OK ... dof=off ...`,
`Failed to compile Material` 0회.

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Scripts/build_reef_m1.py unreal/Aquarium/Content
git commit -m "feat: 해석적 머티리얼 커튼 3장으로 부유 입자 구현 (Niagara 미사용)"
```

---

### Task 8: 프롭 22개와 인스턴스 변형

**Files:** Modify `unreal/Aquarium/Scripts/build_reef_m1.py`

- [ ] **Step 1: 프롭 상수 교체**

```python
PROP_SEED = 77
PROP_COUNT = 22
# (mesh path, scale range) -- rock_09 is a 15 cm pebble, so it is scaled up a lot
PROP_MESHES = [
    ("/Game/Props/SM_BranchCoral", (0.7, 1.4)),
    ("/Game/Props/SM_PlateCoral",  (0.6, 1.2)),
    ("/Game/Props/SM_BrainCoral",  (0.7, 1.4)),
    ("/Game/Props/SM_FanCoral",    (0.7, 1.5)),
    ("/Game/Props/SM_TubeCoral",   (0.8, 1.6)),
    ("/Game/Props/SM_boulder_01",  (0.8, 1.5)),
    ("/Game/Props/SM_rock_07",     (2.0, 4.0)),
    ("/Game/Props/SM_rock_09",     (4.0, 8.0)),
]
# Unscaled XY half-extents (cm); the two new entries are filled in from the import log in
# Step 3 below. The placement radius is max(x, y) * actor scale.
PROP_HALF_EXTENTS = {
    "SM_BranchCoral": (28.3, 22.2),
    "SM_PlateCoral":  (52.6, 52.8),
    "SM_BrainCoral":  (31.4, 31.4),
    "SM_FanCoral":    (30.0, 6.0),
    "SM_TubeCoral":   (26.0, 26.0),
    "SM_boulder_01":  (63.6, 91.5),
    "SM_rock_07":     (8.4, 16.0),
    "SM_rock_09":     (3.7, 7.2),
}
PROP_X = (150.0, 900.0)
PROP_Y = (-450.0, 450.0)
# Per-instance variation on top of the existing random yaw: a small tilt, a non-uniform Z
# stretch and one of the tint material instances. Without these, eight meshes across 22 props
# read as stamped copies no matter how good each mesh is.
PROP_TILT_DEG = 7.0
PROP_Z_STRETCH = (0.85, 1.20)
PROP_TINT_COUNT = {"coral": 3, "rock": 2}
```

- [ ] **Step 2: 배치 루프 변경** — `prop = spawn(...)` 부분을 아래로 바꾼다.

```python
    yaw = prng.uniform(0.0, 360.0)
    pitch = prng.uniform(-PROP_TILT_DEG, PROP_TILT_DEG)
    roll = prng.uniform(-PROP_TILT_DEG, PROP_TILT_DEG)
    z_stretch = prng.uniform(*PROP_Z_STRETCH)
    kind = "rock" if name.startswith("SM_rock") or name.startswith("SM_boulder") else "coral"
    tint_index = prng.randrange(PROP_TINT_COUNT[kind])
    mi_path = "/Game/Props/MI_%s_v%d" % (name[len("SM_"):], tint_index)

    prop = eas.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(x, y, FLOOR_Z),
        unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    assert prop is not None, "prop spawn failed: %s" % mesh_path
    prop.set_actor_label("Prop_%02d_%s" % (i, name))
    pc = prop.static_mesh_component
    pc.set_mobility(unreal.ComponentMobility.STATIC)
    sm = unreal.load_asset(mesh_path)
    assert isinstance(sm, unreal.StaticMesh), "prop mesh missing: %s" % mesh_path
    assert pc.set_static_mesh(sm), "failed to set prop mesh: %s" % mesh_path
    mi = unreal.load_asset(mi_path)
    assert isinstance(mi, unreal.MaterialInstanceConstant), "prop tint missing: %s" % mi_path
    pc.set_material(0, mi)
    prop.set_actor_scale3d(unreal.Vector(scale, scale, scale * z_stretch))
    prop_count += 1
```
(`spawn()` 헬퍼는 roll을 받지 않으므로 여기서만 `spawn_actor_from_class`를 직접 부른다.)

- [ ] **Step 3: 새 산호 두 종의 실제 half-extent 확인 후 반영**

```bash
UE="/Users/Shared/Epic Games/UE_5.8"
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" \
  -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/import_props.py" \
  -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 \
  | grep "PROPS_OK count"
```
출력의 `('SM_FanCoral', 'extent=(a,b,c)', ...)`와 `('SM_TubeCoral', ...)`에서 a·b를 읽어
`PROP_HALF_EXTENTS`의 두 항목을 실제 값으로 고친다(추정치를 남겨 두면 배치 간격이 어긋난다).

- [ ] **Step 4: 실행**

```bash
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" \
  -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/build_reef_m1.py" \
  -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 \
  | grep -E "REEF_OK|REEF_WARN|Traceback|LogPython: Error|Failed to compile Material"
```
기대: `REEF_OK actors=<n> fish=36 props=22 dof=off map=/Game/Maps/ReefM1`.
`REEF_WARN`이 3개를 넘으면 `PROP_SEPARATION_RELAXED`를 0.70으로 낮춘다.

- [ ] **Step 5: 장면 검증**

```bash
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" \
  -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/verify_scene.py" \
  -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 \
  | grep -E "SCENE_OK|Traceback|AssertionError"
```
`verify_scene.py`가 프롭 개수를 고정 14로 단언하고 있으면 22로 고친다.

- [ ] **Step 6: 커밋**

```bash
git add unreal/Aquarium/Scripts/build_reef_m1.py unreal/Aquarium/Scripts/verify_scene.py unreal/Aquarium/Content
git commit -m "feat: 프롭 22개로 늘리고 기울임·비균일 스케일·틴트 인스턴스 변형 적용"
```

---

### Task 9: 머티리얼 컴파일 가드

**Files:** Create/modify the Unreal automation test source under `unreal/Aquarium/Source/Aquarium/Private/Tests/` (기존 테스트 파일들과 같은 폴더)

M4a에서 같은 사고를 **두 번** 당했다: 머티리얼이 컴파일에 실패해 회색 기본 머티리얼로
그려지는데 에디터 뷰포트는 멀쩡해 보였고, 두 번 다 캡처를 눈으로 보고서야 발견했다.
이 태스크는 그걸 자동으로 잡는다.

- [ ] **Step 1: 기존 테스트 파일 위치 확인**

```bash
ls unreal/Aquarium/Source/Aquarium/Private/Tests/
grep -rn "IMPLEMENT_SIMPLE_AUTOMATION_TEST" unreal/Aquarium/Source/Aquarium/Private/Tests/ | head -3
```

- [ ] **Step 2: 테스트 추가** — 기존 테스트 파일들과 같은 매크로 스타일로 새 파일
`PropMaterialsCompileTest.cpp`를 만든다.

```cpp
// Guards the failure mode that cost M4a two rounds of work: a material that fails to compile is
// silently swapped for the grey Default Material at runtime while the editor viewport still
// looks fine. Every material this project generates from Python lives under /Game/Props or
// /Game/Env, so walking those two folders covers all of them.
#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Materials/Material.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAquariumPropMaterialsCompileTest,
    "Aquarium.Content.PropMaterialsCompile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAquariumPropMaterialsCompileTest::RunTest(const FString& Parameters)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    Registry.Get().SearchAllAssets(/*bSynchronousSearch=*/true);

    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(TEXT("/Game/Props"));
    Filter.PackagePaths.Add(TEXT("/Game/Env"));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    Registry.Get().GetAssets(Filter, Assets);
    TestTrue(TEXT("at least one generated material was found"), Assets.Num() > 0);

    int32 Checked = 0;
    for (const FAssetData& Asset : Assets)
    {
        UMaterial* Material = Cast<UMaterial>(Asset.GetAsset());
        if (Material == nullptr)
        {
            continue;
        }
#if WITH_EDITOR
        const FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIFeatureLevel);
        if (Resource == nullptr)
        {
            // -nullrhi runs may have no material resource at all. Say so instead of passing
            // quietly: the log grep in scripts/render_m4b_compare.sh is the real gate.
            AddInfo(FString::Printf(TEXT("%s: no material resource (nullrhi?), skipped"),
                                    *Asset.AssetName.ToString()));
            continue;
        }
        const TArray<FString>& Errors = Resource->GetCompileErrors();
        TestEqual(FString::Printf(TEXT("%s compile errors"), *Asset.AssetName.ToString()),
                  Errors.Num(), 0);
        for (const FString& Error : Errors)
        {
            AddError(FString::Printf(TEXT("%s: %s"), *Asset.AssetName.ToString(), *Error));
        }
        ++Checked;
#endif
    }
    AddInfo(FString::Printf(TEXT("checked %d of %d materials"), Checked, Assets.Num()));
    return true;
}
```

- [ ] **Step 3: 빌드(두 번)와 실행**

```bash
UE="/Users/Shared/Epic Games/UE_5.8"
for pass in 1 2; do
  "$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development \
    -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -E "Result:|error:"
done
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" \
  -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi \
  -stdout -FullStdOutLogOutput 2>&1 | grep "Test Completed"
```
기대: `Test Completed. Result={Success}. Name={...} ... 38` — 즉 37개 → **38개** 전부 Success.
`AssetRegistryModule`/`ARFilter` 헤더가 모듈 의존성에 없으면 `Aquarium.Build.cs`의
`PrivateDependencyModuleNames`에 `"AssetRegistry"`를 추가한다.

- [ ] **Step 4: 커밋**

```bash
git add unreal/Aquarium/Source
git commit -m "test: 프롭·환경 머티리얼 컴파일 오류 0 단언 자동화 테스트 추가"
```

---

### Task 10: 비교 산출물 하네스

**Files:** Create `scripts/render_m4b_compare.sh`

- [ ] **Step 1: 스크립트 작성** — `scripts/render_m4a_compare.sh`를 복사해 아래 네 가지를 바꾼다.

```bash
cp scripts/render_m4a_compare.sh scripts/render_m4b_compare.sh
chmod +x scripts/render_m4b_compare.sh
```

1. 헤더 주석과 모든 `m4a` 산출물 이름을 `m4b`로 바꾼다. 근접 스틸 이름은
   `$DATE-m4b-coral.png`, 임시 레벨 이름은 `/Game/Maps/_CoralTmp`,
   `TMP_MAP_PKG="$ROOT/unreal/Aquarium/Content/Maps/_CoralTmp.umap"`.
2. `BEFORE` 기본값:
```bash
BEFORE="${BEFORE:-$REVIEWS/2026-09-21-m4a-scene.png}"
```
3. 근접 스틸의 임베드 Python에서 물고기 스폰 블록 전체를 산호 다섯 개 스폰으로 바꾼다
   (`SPECIES`/`CLOSEUP_SPECIES` 관련 변수는 제거하고 `CLOSEUP_FOV` 기본값은 45로 올린다):

```python
CORALS = ["BranchCoral", "PlateCoral", "BrainCoral", "FanCoral", "TubeCoral"]
for i, name in enumerate(CORALS):
    mesh_path = "/Game/Props/SM_%s" % name
    sm = unreal.load_asset(mesh_path)
    assert isinstance(sm, unreal.StaticMesh), "coral mesh missing: %s" % mesh_path
    prop = spawn(unreal.StaticMeshActor, (150.0, Y - 130.0 + i * 65.0, 0.0),
                 (0.0, 25.0 * i), label="Coral_%s" % name)
    pc = prop.static_mesh_component
    pc.set_mobility(unreal.ComponentMobility.STATIC)
    assert pc.set_static_mesh(sm), "failed to set coral mesh: %s" % mesh_path
    mi = unreal.load_asset("/Game/Props/MI_%s_v0" % name)
    assert mi is not None, "coral tint missing for %s" % name
    pc.set_material(0, mi)

floor = spawn(unreal.StaticMeshActor, (150.0, Y, 0.0), label="SandPatch")
fsc = floor.static_mesh_component
fsc.set_mobility(unreal.ComponentMobility.STATIC)
assert fsc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
fsc.set_material(0, unreal.load_asset("/Game/Env/M_Sand"))
floor.set_actor_scale3d(unreal.Vector(8.0, 8.0, 1.0))

print("CORAL_LEVEL_OK %s corals=%d fov=%.1f" % (MAP, len(CORALS), FOV))
```
   카메라는 `spawn(unreal.CameraActor, (CAM_X, Y, 60.0), (-8.0, 0.0), label="DiverCamera")`로
   낮추고, `CAM_X = -180.0`(산호 무리에서 약 3.3 m 뒤)로 둔다. 안개·태양·스카이 블록은 그대로
   두되 안개 밀도는 `build_reef_m1.py`의 새 값(2.2)으로 맞춘다.
4. 마지막에 로그 grep 단언을 추가한다:

```bash
# --- Step 4: material compile guard -------------------------------------------
ENGINE_LOG="$HOME/Library/Logs/Aquarium/Aquarium.log"
FAILED=0
if [[ -f "$ENGINE_LOG" ]]; then
  FAILED="$(grep -c "Failed to compile Material" "$ENGINE_LOG" || true)"
fi
echo "MATERIAL_COMPILE_FAILURES=$FAILED"
if [[ "$FAILED" != "0" ]]; then
  echo "ERROR: $FAILED material(s) failed to compile - they are being drawn as the grey Default Material" >&2
  grep "Failed to compile Material" "$ENGINE_LOG" | head -20 >&2
  exit 1
fi
```

- [ ] **Step 2: 실행**

```bash
scripts/render_m4b_compare.sh 2>&1 | tail -30
```
기대: `CORAL_LEVEL_OK`, `CLOSEUP_OK`, `VIDEO_OK`, `SCENE_OK`, `COMPARE_OK`,
`MATERIAL_COMPILE_FAILURES=0`.

- [ ] **Step 3: 캡처를 Read 도구로 직접 본다** — 네 장 전부:
  `docs/reviews/<날짜>-m4b-coral.png`, `-scene.png`, `-compare.png`, 그리고 위 `$B/before-scene.png`.
  설계 문서의 판정 기준 5개를 하나씩 확인한다:
  1. 산호가 흰 덩어리가 아닌가
  2. 빛줄기가 보이는가
  3. 20 m 배경 물고기의 색·종이 구분되는가
  4. 부유 입자가 보이되 플레이어 물고기를 가리지 않는가
  5. 같은 산호가 같은 각도·같은 색으로 반복되지 않는가

  하나라도 아니면 해당 태스크의 수치로 돌아가 고치고 다시 찍는다(최대 3회).
  - 1번이 안 되면 Task 3의 `bump_strength`·`detail` `sharpness`
  - 2번이 안 되면 Task 5의 `GOBO_THRESHOLD`(0.30~0.55)와 `SUN_VOLUMETRIC_SCATTERING`
  - 3번이 안 되면 Task 5의 `FOG_DENSITY`(1.6~2.6)
  - 4번이 안 되면 Task 7의 `Brightness`·`DotRadius`
  - 5번이 안 되면 Task 4의 `CORAL_TINTS` 대비를 키운다

- [ ] **Step 4: 커밋**

```bash
git add scripts/render_m4b_compare.sh docs/reviews
git commit -m "feat: M4b 비교 산출물 하네스와 머티리얼 컴파일 로그 가드"
```

---

### Task 11: 성능 재측정

**Files:** 산출물 `docs/reviews/<날짜>-m4b-{frametimes.csv,perf.md}`

- [ ] **Step 1: 측정**

```bash
SKIP_BUILD=1 scripts/measure_m2b_perf.sh 2>&1 | tail -5
```
기대: `PERF_OK samples=<n> avg_fps=<x> p95_ms=<y>`.

- [ ] **Step 2: 산출물 이름 정리** — `measure_m2b_perf.sh`는 이름을 항상 `m2b`로 쓴다
(M4a에서와 같은 처리).

```bash
DATE="$(date +%F)"
mv "docs/reviews/$DATE-m2b-frametimes.csv" "docs/reviews/$DATE-m4b-frametimes.csv"
mv "docs/reviews/$DATE-m2b-perf.md" "docs/reviews/$DATE-m4b-perf.md"
```
그리고 `docs/reviews/<날짜>-m4b-perf.md`의 제목을 `# M4b 성능 측정 (<날짜>)`으로 고치고,
"## 기준선 비교" 절을 직접 덧붙인다: M2b 평균 102.5 fps / p95 10.22 ms,
M4a 평균 100.9 fps / p95 10.27 ms, M4b 측정값, 그리고 이번에 추가된 비용 항목(볼류메트릭 격자
4/128, 반투명 커튼 3장, 프롭 22개, SSAO)을 적는다.

- [ ] **Step 3: 예산 판정** — 평균 60 fps 이상이면 그대로 진행한다. **60 fps 미만이면**
설계 문서의 컷 목록을 **한 번에 하나씩** 되돌리고 그때마다 Step 1을 다시 돌린다:
  1. `DefaultEngine.ini`의 `r.VolumetricFog.GridPixelSize=8`, `GridSizeZ=64`
  2. `SNOW_CURTAINS`에서 `near` 항목 제거 (3장 → 2장)
  3. `FOG_VOLUMETRIC_DISTANCE` 6000 → 4000
  4. `PROP_COUNT` 22 → 16
  5. `PP_AO_INTENSITY` 0.0 (AO 끔)
  무엇을 잘랐고 그때 수치가 얼마였는지 `-perf.md`에 남긴다. 컷을 적용했으면 Task 10 Step 2~3을
  다시 돌려 캡처를 새로 만든다.

- [ ] **Step 4: 커밋**

```bash
git add docs/reviews
git commit -m "perf: M4b 프레임 시간 재측정과 기준선 비교"
```

---

### Task 12: 문서와 마무리

- [ ] **Step 1: 전체 재검증**

```bash
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure 2>&1 | tail -3
UE="/Users/Shared/Epic Games/UE_5.8"
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" \
  -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi \
  -stdout -FullStdOutLogOutput 2>&1 | grep -c "Result={Success}"
```
기대: `100% tests passed out of 66`, Automation Success `38`.

- [ ] **Step 2: 문서 갱신**
  - `docs/TASK.md`: M4b 행을 M4a 행과 같은 형식으로 채운다(산호 5종·PBR 3장, 프롭 22개·인스턴스
    변형, 수면 차폐물 빛줄기, 안개 재튜닝, 후처리 색보정·블룸·AO·심도 끔, 머티리얼 커튼 부유 입자,
    규칙 66 + Automation 38, 성능 수치, 산출물 경로, **사용자 시각 검토: 대기**). "검증 현황"에
    M4b 시각 검토 항목과 알려진 품질 한계를 추가한다. M1의 "빛줄기 약함" 한계가 해소됐음을 적는다.
  - `docs/SETUP.md`: 재현 명령 제목을 "M1~M4b 재현 명령"으로 바꾸고, 8단계로
    `scripts/render_m4b_compare.sh`를 추가한다. 산호 스크립트가 이제 5종 × 3장을 굽는다는 점,
    `import_props.py`가 `MI_*` 틴트 인스턴스를 만든다는 점,
    `MATERIAL_COMPILE_FAILURES=0` 확인이 하네스에 들어갔다는 점을 적는다.
  - `docs/ASSETS.md`: "직접 제작" 표에 `FanCoral`·`TubeCoral` 행을 추가하고, 산호 3종의
    산출물 열을 3장(`T_<N>_{BaseColor,Normal,Roughness}.png`, 각 1024²)으로 갱신한다.
    "산호 비고"에 5종·미리보기 x 좌표·PBR 전환을 적는다. 외부 에셋 표는 바뀌지 않았음을
    M4a와 같은 형식으로 확인해 적는다(바위는 원본 파일도 임포트 설정도 그대로, 틴트는 머티리얼
    인스턴스라 원본 수정이 아니다).
  - `README.md`: 상태 줄을 M4b까지로 갱신.

- [ ] **Step 3: 자체 검토** — 커밋 전에 세 가지를 확인한다.
  1. 설계 문서의 결정 표 항목마다 대응하는 태스크가 있는가
  2. `grep -rn "TBD\|FIXME\|TODO" docs/superpowers/*/2026-09-21-m4b-*.md assets/blender/make_corals.py unreal/Aquarium/Scripts/*.py` 가 비어 있는가
  3. `CORALS` 목록, `PROP_MESHES`, `PROP_HALF_EXTENTS`, `MI_*` 경로의 이름이 세 스크립트에서 일치하는가

- [ ] **Step 4: 커밋·푸시**

```bash
git add docs README.md
git commit -m "docs: M4b 결과 기록 (산호 5종 PBR, 빛줄기, 후처리, 부유 입자)"
git push -u origin feat/m4b-reef-lighting
```

- [ ] **Step 5: 사용자에게 `-compare.png`·`-coral.png`·`-reef.mp4` 전달 후
superpowers:finishing-a-development-branch**

---

## 범위 밖 (다음 계획)

- M4c: 무리(보이즈) 행동, 소품 충돌, 수직 전환 롤 제거.
- M5 클릭 도망, M6 성능 판정·패키징.
- M4a 이월 항목(비늘 이방성 셀, 가슴지느러미 위치, 나비고기 `pecStray=16`, 고정 바운드
  `BoundsScale`, 미사용 `SK_*_PhysicsAsset`)은 물고기를 다시 건드리는 일이라 M4b에서 하지 않는다.

---

## 구현 중 발견한 후속 항목

- **TubeCoral이 산호가 아니라 말뚝 울타리(바코드)로 읽힌다** — `2026-09-21-m4b-compare.png` 오른쪽에서
  확인했다. 세로 막대가 줄지어 선 형태라 산호로 보이지 않는다. 머티리얼이 아니라 **형상 품질** 문제이므로
  `make_corals.py`의 관 배치(반경·높이 변주, 기울기, 관 끝 개구부)를 다시 잡아야 한다. 한 줄 수정이 아니라
  재생성 + 재임포트가 필요해 이월한다.
- **FanCoral의 실루엣이 BranchCoral과 거의 같다** — 게임 거리에서 두 종이 구분되지 않아 산호초가
  "5종"이 아니라 "4종 + 변종 하나"로 읽힌다. 부채 산호는 얇은 판상 망으로 가야 하는데 지금은 가지 구조에
  가깝다. TubeCoral과 함께 다음 산호 반복에서 형상을 다시 만든다.
- **PlateCoral·BrainCoral이 여전히 흰 덩어리로 읽힌다** — 원인은 `CORAL_TINTS[0]`이 항등 틴트
  `(1,1,1)`이고 다섯 종이 이 목록을 공유한다는 점이다. 종별 색을 주려면 종→틴트 딕셔너리를 새로 만들고
  재임포트해야 하므로 한 줄 수정이 아니다. **M4b가 만든 퇴행이 아니라 이전부터 있던 상태**임을
  HEAD 버전과 비교해 확인했다.
- **모래 바닥이 전체적으로 약간 밝아졌다** — 스카이라이트 0.8 → 2.2 상향의 부수 효과다. 전경 물고기를
  거의 검은 실루엣에서 건져내는 것이 목적이었으므로 그대로 두었다. 더 가라앉은 톤을 원하면
  스카이라이트를 **2.2 → 1.6~1.8**로 내리는 한 줄이 조절 손잡이다.
- **빛줄기가 커밋 `105e4c8` 시점보다 아주 약간 부드럽다** — `GOBO_COARSE_THRESHOLD`를 0.44 아래로
  내리면 다시 또렷해지지만 바닥이 그만큼 어두워진다. 지금 값은 빛줄기 대비와 바닥 밝기의 절충이며,
  사용자 판정 후 조정한다.
- **`PropMaterialsCompile`의 남은 한계** — 셰이더 컴파일 오류는 `-nullrhi`에 `FMaterialResource`가
  없어 헤드리스에서 검사되지 않는다(로그에 `0 of 13 material resources`로 드러난다). 샘플러 타입·압축
  설정 불일치(샘플러 51개)는 RHI 없이도 잡지만, **셰이더 컴파일 실패의 1차 방어선은 여전히 실제 게임
  실행 로그 grep**이다. 이 사실은 숨기지 않고 TASK.md·SETUP.md에도 같이 적었다.
- **M3에서 이월** — F-06(포커스 상실 일시정지)의 실제 윈도 포커스 델리게이트는 헤드리스에서 발생하지
  않는다. 사용자가 창 모드로 다른 앱에 전환했다가 돌아오는 수동 확인 1회가 여전히 필요하다.
- **M4a에서 이월(그대로 열려 있음)** — 비늘 이방성 셀, 가슴지느러미 위치·크기, 나비고기만
  `pecStray=16`, 고정 바운드의 `BoundsScale`, 미사용 `SK_*_PhysicsAsset`. 물고기를 다시 건드리는
  일이라 M4b 범위 밖이다. 상세는 [M4a 구현 계획](2026-09-21-m4a-fish-fidelity.md)의 같은 절.
