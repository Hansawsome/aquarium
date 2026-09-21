# M4b 설계 — 산호·바위 품질과 조명·후처리 (실사 반복 2단계)

작성일: 2026-09-21 · 상태: 사용자 위임, 결정은 이 문서에 기록

## 왜 이것이 다음인가

M4a에서 물고기는 눈·막 지느러미·PBR 3장으로 확실히 좋아졌다. 그런데 같은 캡처
(`docs/reviews/2026-09-21-m4a-scene.png`)를 보면 물고기 말고는 전부 그대로다:

- **산호가 하얀 덩어리다.** 뇌 산호·판상 산호는 흰 혹과 흰 웅덩이로 읽히고, 가지 산호만
  희미한 분홍 막대다. 원인은 둘이다 — 베이크한 베이스 컬러 한 장뿐이라 표면에 빛이 걸릴
  굴곡이 없고, 러프니스가 상수 0.6이라 전부 같은 재질로 보인다.
- **빛줄기가 없다.** M1에서 "빛줄기 약함"으로 적은 한계가 그대로다. 볼류메트릭 포그는 켜져
  있고 태양의 `volumetric_scattering_intensity`도 8.0인데 화면에 줄기가 하나도 없다.
  이유는 세기가 아니라 **가릴 것이 없어서**다. 신(scene)에서 카메라 위쪽에 그림자를 드리우는
  지오메트리가 하나도 없으므로 빛이 균일하게 산란할 뿐 줄기가 생기지 않는다.
- **10 m면 색이 다 날아간다.** `FOG_DENSITY = 5.0`, `extinction_scale = 1.0`이라 배경
  물고기 36마리가 단색 실루엣으로 뭉갠다. M2b에서 한 군집 작업이 화면에서 보이지 않는
  이유가 이것이다.
- **물속인데 물속 같지 않다.** 부유 입자가 없어 빈 공간이 완전히 비어 있다.

목표는 M4a와 같다: 사진이 아니라 **"산호초 같이 보이는" 수준**. 판정 방법도 그대로다 —
같은 맵·같은 시드·같은 카메라·같은 `-AquariumAutoInput` 패턴·같은 t의 스틸을 M4a 캡처와
나란히 놓고 본다. "전" 이미지는 `docs/reviews/2026-09-21-m4a-scene.png`다.

## 결정 사항 (아쿠부장 판단, 2026-09-21)

| 항목 | 결정 | 이유 |
|---|---|---|
| 산호 재질 | 물고기와 **같은 대우**: `fishlib.bake_maps`로 BaseColor + Normal + Roughness 세 장을 1024²로 굽는다. 종마다 표면 디테일을 다르게 준다 — 뇌 산호는 2차 미세 고랑, 가지 산호는 폴립 돌기, 판상 산호는 동심 융기, 부채 산호는 격자결, 관 산호는 가로 띠 | 하얀 덩어리의 원인은 색이 아니라 굴곡이다. 노멀·러프니스가 들어가면 코스틱과 태양이 표면을 타고 흐르면서 형태가 읽힌다. 물고기에서 이미 검증된 경로라 새 위험이 없다 |
| 산호 생성 방식 | 절차적 Blender 생성 유지. 다운로드 모델로 바꾸지 않는다 | 재현성이 이 프로젝트의 원칙이다. `.blend`/`.fbx`/텍스처는 전부 스크립트 산출물이어야 한다 |
| 산호 종류 수 | **3종 → 5종.** `FanCoral`(부채 산호, 얇은 격자 판)과 `TubeCoral`(관 산호, 수직 관 다발)을 추가한다 | 5종이면 프롭 22개에서 같은 실루엣이 평균 4~5번 나온다. 7종 이상은 Blender 빌드 시간과 텍스처 메모리만 늘리고 10~20 m 거리에서 구분되지 않는다. 실루엣 계열을 "가지·판·혹" 셋에서 "가지·판·혹·부채·관" 다섯으로 넓히는 것이 효율이 가장 크다 |
| 프롭 개수 | **14개 → 22개.** 메시 8종(산호 5 + 바위 3)을 한 번씩 깐 뒤 14개를 무작위로 채운다 | 지금 14개는 40 m 바닥에 듬성하다. 22개는 정적 불투명 메시이고 바위는 LOD 체인이 있어 현재 100.9 fps 여유 안에서 감당된다. 60 fps 미만이면 컷 목록 순서대로 되돌린다 |
| 인스턴스 변형 | 프롭마다 (1) 요(yaw) 0~360°(기존), (2) 피치·롤 ±7° 기울임, (3) **비균일 스케일**(Z를 0.85~1.20배), (4) **색 틴트** — 종마다 `MaterialInstanceConstant` 3개(바위는 2개)를 만들어 액터별로 배정 | 같은 메시가 같은 각도·같은 색으로 반복되면 "찍어낸" 느낌이 난다. 기울임과 Z 스케일은 비용이 0이고, 틴트는 MIC라 드로우콜만 늘지 배칭 손해가 거의 없다. 런타임 MID를 쓰지 않는 이유는 레벨이 저장되는 에셋이라 MID가 남지 않기 때문이다 |
| 바위 | **Poly Haven CC0 그대로.** 새로 받지 않는다. 회전·스케일·틴트 변형만 적용 | M4a에서 `TC_MASKS` 수정으로 머티리얼이 컴파일되기 시작한 뒤로 지금 캡처에서 가장 잘 나오는 요소다. 건드릴 이유가 없다 |
| 빛줄기 | 세기를 올리는 게 아니라 **차폐물을 만든다.** 수면 높이(Z = 1100)에 6000 cm 평면 `SurfaceGobo`를 깔고, 코스틱과 같은 해석적 파동을 불투명 마스크로 쓰는 `M_SurfaceGobo`(Masked, Unlit)를 입힌다. 이 평면은 `set_visibility(False)` + `bCastHiddenShadow = True`로 **보이지 않으면서 그림자만 드리운다**. 태양 `volumetric_scattering_intensity` 8.0 → 14.0 | 볼류메트릭 god ray는 "산란 세기 × 그림자 대비"다. 지금 장면에는 태양을 가리는 지오메트리가 위쪽에 하나도 없어서 대비가 0이고, 그래서 세기를 아무리 올려도 화면 전체가 균일하게 밝아질 뿐이다. 보이지 않는 차폐물은 UE가 정식으로 지원하는 기능(`bCastHiddenShadow`)이라 하늘에 이상한 판이 보일 위험도 없다 |
| 볼류메트릭 품질 | `DefaultEngine.ini`에 `r.VolumetricFog.GridPixelSize=4`, `r.VolumetricFog.GridSizeZ=128` | 기본값(8 / 64)에서는 줄기 경계가 계단처럼 뭉개진다. 이 두 값은 성능에 직접 걸리므로 **컷 목록 1순위**다 |
| 안개 | `FOG_DENSITY` 5.0 → **2.2**, `start_distance` 0 → 150 cm, `volumetric_fog_extinction_scale` 1.0 → 0.7, `FOG_INSCATTER`를 더 짙은 청록(0.015, 0.105, 0.175)으로, `directional_inscattering_exponent`를 6.0으로 낮춰 태양 쪽 글로우를 넓힌다 | 지금 수치는 10 m에서 색을 전부 먹는다. 밀도를 낮추면 20 m 배경 물고기의 종·색이 남고, 인스캐터를 어둡게 하면 "멀리 = 밝은 하늘색"이 아니라 "멀리 = 짙은 물빛"이 되어 깊이가 오히려 더 읽힌다. `start_distance`는 카메라 바로 앞 1.5 m를 맑게 비워 플레이어 물고기를 선명하게 한다 |
| 색보정 | 언바운드 `PostProcessVolume` 하나에 채도 1.15, 콘트라스트 1.06, 색온도 5200 K(약간 따뜻하게), 블룸 0.5 / 임계 1.0, SSAO 강도 0.5 / 반경 80 cm | 안개를 낮추면 대비가 함께 떨어진다. 색보정은 그걸 되돌리는 가장 싼 수단이고, 따뜻한 색온도는 화면 전체가 파랗게 기우는 것을 상쇄한다. AO는 산호 고랑과 바위 틈을 앉히는 데 노멀맵 다음으로 효과가 크다 |
| 심도(DoF) | **기본 끔.** `depth_of_field_fstop = 32`, `depth_of_field_focal_distance = 0`을 명시적으로 override해 프로젝트 설정이 바뀌어도 켜지지 않게 못 박는다. 켜고 싶을 때를 위해 `build_reef_m1.py` 상단에 `DOF = None` 상수 블록을 두고, dict를 넣으면 켜지게 한다 | 카메라가 고정이고 아이가 보는 대상(자기 물고기)이 화면 아래쪽 근거리에 있다. DoF를 켜면 아이가 보려는 바로 그것이 흐려질 위험이 실제로 있고, 프레임 시간도 먹는다. 런타임 플래그로 만들지 않는 이유는 개발 전용 플래그가 하나 늘어날 값어치가 없기 때문이다 — 스크립트 상수면 충분하고 비용이 0이다 |
| 부유 입자 | **Niagara를 쓰지 않는다.** 대신 카메라 앞 X = 90 / 240 / 480 cm에 양면 반투명 Unlit 평면 3장(`SnowCurtain_0..2`)을 세우고, 해석적 셀 해시로 점을 찍는 `M_MarineSnow`를 입힌다. 커튼마다 셀 크기·밝기·드리프트가 다른 MIC를 쓴다 | **Niagara는 에디터 Python으로 재현 가능하게 만들 수 없다.** `NiagaraSystemFactoryNew`로 빈 시스템은 만들 수 있지만 에미터·모듈 그래프를 Python에서 구성하는 API가 없어서, 손으로 만든 `.uasset`을 커밋하는 수밖에 없다. 그건 "`.uasset`을 손으로 편집하지 않는다"는 이 프로젝트의 제1 제약과 정면으로 충돌한다. 머티리얼 그래프는 `MaterialEditingLibrary`로 완전히 스크립트화되고(이미 `M_Caustics`가 40여 노드짜리 해석적 그래프다), **카메라가 고정**이라 커튼 평면이 카메라를 따라다닐 필요도 없다. 재현성이 충실도보다 우선이라는 지침 그대로의 선택이다 |
| 성능 | `scripts/measure_m2b_perf.sh`를 그대로 돌려 M4a 기준선(평균 100.9 fps / p95 10.27 ms)과 비교한다. 평균 60 fps 미만이면 **컷 목록 순서대로** 되돌리고 재측정 | 볼류메트릭 격자와 반투명 커튼이 예산을 깨뜨릴 가장 유력한 두 가지다. 무엇을 자를지 미리 정해 두지 않으면 측정 후에 즉흥적으로 타협하게 된다 |
| 자동 테스트 | M4b는 대부분 시각 작업이라 **새 규칙 테스트는 없다**(규칙 66개 유지). Unreal Automation은 1개만 늘린다 — `/Game/Props`·`/Game/Env`의 머티리얼 컴파일 오류가 0인지 단언하는 에디터 전용 테스트 | 없는 테스트를 지어내지 않는다. 다만 M4a에서 두 번 잃은 "머티리얼이 조용히 회색 기본 머티리얼로 떨어지는" 사고만은 자동으로 잡을 값어치가 있다. 이 테스트는 `-nullrhi`에서 머티리얼 리소스를 못 얻으면 스킵되므로 **주 방어선은 로그 grep**이고 테스트는 보조다(아래 검증 표 참고) |

### 컷 목록 (평균 60 fps 미만일 때 이 순서로 되돌린다)

1. `r.VolumetricFog.GridPixelSize` 4 → 8, `GridSizeZ` 128 → 64
2. 근거리 커튼(`SnowCurtain_0`, X = 90) 제거 — 3장 → 2장
3. `FOG_VOLUMETRIC_DISTANCE` 6000 → 4000
4. `PROP_COUNT` 22 → 16
5. SSAO 끔

### 검토한 대안

- **Niagara 부유 입자** — 위 표대로 Python 재현이 불가능해 탈락.
- **산호를 Sketchfab/Quixel 무료 모델로 교체** — 절차 생성 원칙 위반이고, 라이선스·스케일·원점
  규약을 5종에 맞추는 비용이 직접 만드는 비용보다 크다.
- **태양 세기만 올려 빛줄기 해결** — 차폐물이 없으면 대비가 생기지 않아 화면만 밝아진다. 실제로
  지금 값(8.0)도 이미 낮지 않다.
- **Light Shaft Bloom / Occlusion** — 태양이 화면 안에 있어야 동작하는데 카메라는 수평에서
  -4°를 보고 있어 태양이 프레임 밖이다.
- **프롭을 `InstancedStaticMeshComponent`로 묶기** — 22개 규모에서 드로우콜 이득이 미미하고,
  인스턴스별 틴트를 주려면 per-instance custom data가 필요해 오히려 복잡해진다.
- **런타임 `-AquariumDepthOfField` 개발 플래그** — 플래그 하나 늘릴 값어치가 없다. 스크립트 상수로 충분.
- **안개를 낮추는 대신 배경 물고기를 카메라 쪽으로 당기기** — M2b/M3의 배치 시드가 바뀌어 전/후
  비교가 성립하지 않는다.

## 구성 요소

### `assets/blender/fishlib.py` (확장)

- `ridge_pattern(nt, axis, period, sharpness=1.0)` — 한 축(`'X'`/`'Y'`/`'Z'`)을 따라 주기
  `period` cm로 반복되는 융기 패턴. `(height_socket, roughness_socket)`을 돌려주며
  `scale_pattern_world`와 같은 계약이다. 관 산호의 가로 띠와 판상 산호의 동심 융기에 쓴다.
- `radial_ridge_pattern(nt, period, sharpness=1.0)` — 원점으로부터의 XY 반경을 따라 반복되는
  융기. 판상 산호 전용. 같은 반환 계약.

기존 `build_body`, `add_fin`, `bake_base_color`(레거시 경로)는 그대로 둔다 — 다만 M4b 이후
산호가 `bake_maps`로 옮겨 가면 `bake_base_color`를 쓰는 호출자는 없어진다. 삭제는 하지 않는다
(M4c까지 회귀 대비).

### `assets/blender/make_corals.py`

- 기존 `build_branch_coral`, `build_plate_coral`, `build_brain_coral` 유지.
- `build_fan_coral(name, seed, width, height, thickness)` — XZ 평면 안에서만 가지를 치는
  재귀 격자. 이웃 가지 끝을 이어 붙여 그물처럼 만들고 Solidify로 얇은 두께를 준다.
- `build_tube_coral(name, seed, count, height, radius)` — 밑동에서 퍼지는 수직 관 다발.
  각 관은 위로 갈수록 살짝 벌어지고 끝이 열려 있다.
- `coral_maps_fn(base, shade, detail)` — `bake_maps`용 `build_nodes(nt, bsdf, ctx)` 콜백
  팩토리. 기존 노이즈 색 혼합에 종별 `detail(nt, ctx)` 패턴을 얹어 Base Color·Roughness·Bump를
  모두 연결한다.
- `build_coral(spec)`는 `F.bake_base_color` 대신 `F.bake_maps(..., size=1024)`를 호출한다.
- 미리보기는 5종을 나란히 놓은 `preview_corals.png` 한 장으로 유지한다(x 간격 조정).

### `unreal/Aquarium/Scripts/import_props.py`

- `CORALS`에 `FanCoral`, `TubeCoral` 추가.
- `import_coral`이 세 장을 임포트한다: `T_<N>_BaseColor`(sRGB), `T_<N>_Normal`(sRGB off,
  `TC_NORMALMAP`, `SAMPLERTYPE_NORMAL`), `T_<N>_Roughness`(sRGB off, **`TC_MASKS`**,
  `SAMPLERTYPE_MASKS`).
- `M_<N>`에 `Tint`라는 `MaterialExpressionVectorParameter`를 추가해 베이스 컬러에 곱한다.
  바위 `M_<rid>`에도 같은 `Tint`를 추가한다.
- `make_tint_instances(mat, name, tints)` — 머티리얼마다 `MI_<name>_v0..vN`
  (`MaterialInstanceConstant`)을 만들고 `Tint` 벡터 파라미터를 설정해 저장한다. 산호 3개씩,
  바위 2개씩.
- `PROPS_OK` 줄에 머티리얼 인스턴스 목록을 추가로 출력한다.

### `unreal/Aquarium/Scripts/build_reef_m1.py`

- 튜닝 상수 갱신: `FOG_*`, `SUN_VOLUMETRIC_SCATTERING`, `PROP_COUNT`, `PROP_MESHES`,
  `PROP_HALF_EXTENTS`, `PROP_X`/`PROP_Y` 확장.
- `GOBO_*` 상수와 `build_surface_gobo(m)` — `M_Caustics`와 같은 파동 합을 불투명 마스크로
  쓰는 Masked·Unlit 머티리얼. 임계값 `GOBO_THRESHOLD`로 구멍 비율을 조절한다.
- `SurfaceGobo` 액터: `/Engine/BasicShapes/Plane`, Z = `GOBO_Z`, 스케일 `GOBO_SCALE`,
  `set_visibility(False)`, `set_cast_hidden_shadow(True)`, 모빌리티 STATIC.
- `build_marine_snow(m)` — 해석적 셀 해시 점 필드. 스칼라 파라미터 `CellSize`, `Drift`,
  `DotRadius`, `Brightness`를 노출한다. Translucent + Unlit + 양면.
- `SnowCurtain_0..2` 액터 3장 + `MI_MarineSnow_near/mid/far` 3개.
- 언바운드 `PostProcessVolume` 하나 (`unbound = True`, `priority = 1.0`): 색보정·블룸·AO,
  그리고 DoF 명시적 끔.
- 프롭 배치 루프에 기울임·Z 스케일·틴트 MIC 배정 추가. `PROP_SEED`는 바꾸지 않는다.

### `unreal/Aquarium/Config/DefaultEngine.ini`

`[/Script/Engine.RendererSettings]`에 `r.VolumetricFog.GridPixelSize=4`,
`r.VolumetricFog.GridSizeZ=128` 추가.

### Unreal Automation (C++)

`FAquariumPropMaterialsCompileTest` 1개 — `/Game/Props`, `/Game/Env`의 모든 `UMaterial`을
로드해 `#if WITH_EDITOR` 경로로 컴파일 오류 수가 0인지 단언한다. 머티리얼 리소스를 얻지
못하는 환경에서는 `AddInfo`로 스킵 사유를 남기고 통과시킨다(그 경우 로그 grep이 유일한 방어선이다).

### `scripts/render_m4b_compare.sh`

`render_m4a_compare.sh`와 같은 구조. 다른 점 세 가지:
1. 근접 스틸이 물고기가 아니라 **산호 근접**이다 — 임시 레벨 `/Game/Maps/_CoralTmp`에 산호
   5종을 1.5 m 앞에 늘어놓고 찍는다(`<날짜>-m4b-coral.png`).
2. `BEFORE` 기본값이 `docs/reviews/2026-09-21-m4a-scene.png`다.
3. 실행 후 **게임 로그에서 `Failed to compile Material`이 0인지 단언**하고, 0이 아니면
   0이 아닌 종료 코드로 끝난다.

## 검증

| 계층 | 검증 |
|---|---|
| 규칙 계층 | **새 테스트 없음.** 66개 그대로 통과해야 한다. M4b는 규칙 계층을 건드리지 않으므로 여기서 새 테스트를 만드는 것은 빈 테스트다 |
| 에셋 계약 | 산호 5종이 `CORALS_OK` 한 줄을 내고, 종마다 정점 수가 `check_export_ready`의 범위 안, 치수가 spec의 `dims` 범위 안 |
| 텍스처 | 산호 5종 × 3장이 1024²로 생성되고 `bake_maps`의 `_assert_not_degenerate`를 통과(노멀맵 픽셀 분산 > 1e-6) |
| 머티리얼 컴파일 | **(a)** `import_props.py`·`build_reef_m1.py` 실행 출력에 `Failed to compile Material`이 0회, **(b)** 캡처 실행 뒤 `~/Library/Logs/Aquarium/Aquarium.log`에 `Failed to compile Material`이 0회, **(c)** Automation 38개 통과(신규 `PropMaterialsCompile` 포함, `-nullrhi`에서 스킵될 수 있음). (a)/(b)가 주 방어선이다 — M4a에서 두 번 다 **캡처를 눈으로 보고서야** 발견했기 때문에, 이제 grep으로 못 박는다 |
| Unreal | 기존 Automation 37개 유지. `verify_scene.py`의 `SCENE_OK`가 프롭 22개·액터 수 증가를 반영 |
| 성능 | `scripts/measure_m2b_perf.sh` 그대로 실행 → `docs/reviews/<날짜>-m4b-perf.md`. M4a 기준선(평균 100.9 fps / p95 10.27 ms) 대비 기록. 평균 60 fps 미만이면 컷 목록 순서대로 되돌리고 재측정 |
| 시각 | 같은 맵·시드·카메라·`-AquariumAutoInput`·같은 t=12로 `<날짜>-m4b-scene.png`를 찍고 `2026-09-21-m4a-scene.png`와 `hstack`해 `-compare.png`. 산호 근접 스틸 `-coral.png`, 20초 클립 `-reef.mp4`. **모든 캡처를 Read 도구로 직접 본다** — 스크립트가 성공했다는 것과 화면이 바뀌었다는 것은 별개다 |

판정 기준(캡처를 볼 때 확인할 것):
1. 산호가 흰 덩어리가 아니라 색과 굴곡이 있는 형태로 읽히는가
2. 물속에 빛줄기가 보이는가
3. 20 m 배경 물고기의 색·종이 구분되는가 (M4a에서는 단색 실루엣)
4. 부유 입자가 보이되 플레이어 물고기를 가리지 않는가
5. 같은 산호가 같은 각도·같은 색으로 반복되지 않는가

## 데이터·오류 처리

- 새 산호 2종의 지오메트리가 `check_export_ready`의 정점·치수 범위를 벗어나면 그 자리에서
  `raise`한다. 5종 중 하나라도 실패하면 나머지를 계속 진행하지 않는다 — 장면 안에서 품질이
  섞이면 전/후 판단이 불가능해진다(M4a와 같은 규약).
- 베이크가 평평하면(`_assert_not_degenerate`) `raise`. `_OK` 마커 규약 유지.
- `import_props.py`는 기존대로 멱등이다. 산호는 제자리 재임포트, 바위는 재생성이므로
  `import_props.py` → `build_reef_m1.py` 순서를 반드시 지킨다(`docs/SETUP.md`와 동일).
- 머티리얼 인스턴스(`MI_*`)는 존재하면 로드해 파라미터만 갱신하고, 없으면 만든다. `_1` 중복
  에셋이 생기면 실패로 본다.
- `SurfaceGobo`가 보이는 채로 렌더되면(하늘에 판이 보이면) `set_visibility(False)`가 적용되지
  않은 것이므로 캡처에서 즉시 실패로 판정한다.
- 커튼 평면이 캡처에 아무것도 보이지 않으면 머티리얼 컴파일 실패이거나 불투명도가 0인
  것이므로, 로그 grep과 `MI_MarineSnow_*`의 `Brightness` 값을 먼저 확인한다.
- 성능이 60 fps 미만이면 컷 목록 순서대로 되돌리고 **되돌릴 때마다 재측정**한다. 한꺼번에
  여러 개를 되돌리면 무엇이 비쌌는지 알 수 없다.

## 범위 밖

- 무리(보이즈) 행동, 소품 충돌, 수직 전환 롤 제거 — M4c.
- 클릭 도망 — M5. 패키징 빌드 기준 성능 판정 — M6.
- 물고기 비늘 이방성 셀, 가슴지느러미 위치, 나비고기 `pecStray=16` 등 M4a 이월 항목은
  M4b에서 다루지 않는다(물고기를 또 건드리면 전/후 비교에서 산호·조명 효과가 가려진다).
- 수면 메시·물 표면 굴절, 흔들리는 해초, 산호의 애니메이션. 전부 M4b 범위 밖이다.
- 바위 신규 다운로드.
