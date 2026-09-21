# 에셋 출처와 제작 기록

무료라는 이유만으로 원본 재배포를 허용한다고 가정하지 않는다.

## 도입한 외부 에셋

| 에셋 | 저작자 | 원본 URL | 다운로드일 | 라이선스 | 출처 표기 | 수정 | 게임 배포 | 공개 저장소 재배포 | 로컬 경로 |
|---|---|---|---|---|---|---|---|---|---|
| Poly Haven `coast_sand_01` (모래 텍스처 2K: diffuse, normal GL, roughness) | Rob Tuytel | https://polyhaven.com/a/coast_sand_01 | 2026-09-20 | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Textures from Poly Haven") | 없음 | 가능 | 가능 | `assets/textures/sand/` |
| Noto Sans KR Regular (한국어 UI 폰트) | Google / Adobe (Noto CJK 프로젝트) | https://github.com/notofonts/noto-cjk | 2026-09-20 | SIL OFL 1.1 (`assets/fonts/OFL.txt`) | 불필요 (OFL: 폰트 단독 판매 금지, 임베드·재배포 허용) | 없음 | 가능 | 가능 | `assets/fonts/` |
| Poly Haven `boulder_01` (바위 모델, FBX 1K: diffuse/normal GL/roughness/AO) | Rico Cilliers | https://polyhaven.com/a/boulder_01 | 2026-09-20 | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Models from Poly Haven") | 없음 | 가능 | 가능 | `assets/models/rocks/boulder_01/` |
| Poly Haven `rock_07` (바위 모델, FBX 1K: diffuse/normal GL/roughness/AO) | Jenelle van Heerden | https://polyhaven.com/a/rock_07 | 2026-09-20 | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Models from Poly Haven") | 없음 | 가능 | 가능 | `assets/models/rocks/rock_07/` |
| Poly Haven `rock_09` (바위 모델, FBX 1K: diffuse/normal GL/roughness/AO) | Jenelle van Heerden | https://polyhaven.com/a/rock_09 | 2026-09-20 | CC0 1.0 (https://polyhaven.com/license) | 불필요 (자발적 표기: "Models from Poly Haven") | 없음 | 가능 | 가능 | `assets/models/rocks/rock_09/` |

M4a 확인(2026-09-21): 위 외부 에셋 5건의 원본 파일(`assets/models/`, `assets/textures/`, `assets/fonts/`)은
M4a 브랜치에서 한 바이트도 바뀌지 않았다(`git diff main...HEAD` 결과 없음). 따라서 저작자·라이선스·수정 여부 열은
그대로 유효하다. M4a에서 바꾼 것은 Unreal 쪽 **임포트 설정**뿐이다 — 바위 러프니스 텍스처를
`TC_MASKS`로 임포트하도록 `unreal/Aquarium/Scripts/import_props.py`를 고쳤다(그 전에는 `TC_Default`라
머티리얼 컴파일이 실패해 바위가 회색 기본 머티리얼로 그려졌다). 원본 에셋의 수정이 아니므로 "수정: 없음"은 유지한다.

## 직접 제작

| 에셋 | 제작 도구 | 생성 스크립트 | 산출물 | 외부 텍스처 |
|---|---|---|---|---|
| 블루탱 (BlueTang) | Blender 5.2.2 LTS | `assets/blender/make_bluetang.py` (+ `fishlib.py`) | `assets/blender/BlueTang.blend`, `assets/blender/export/BlueTang.fbx`, `T_BlueTang_BaseColor.png` / `T_BlueTang_Normal.png` / `T_BlueTang_Roughness.png` (각 2048², 절차적 노드 베이크), `preview_bluetang.png` | 없음 |
| 흰동가리돔 (Clownfish) | Blender 5.2.2 LTS | `assets/blender/make_clownfish.py` (+ `fishlib.py`) | `assets/blender/Clownfish.blend`, `assets/blender/export/Clownfish.fbx`, `T_Clownfish_BaseColor.png` / `T_Clownfish_Normal.png` / `T_Clownfish_Roughness.png` (각 2048², 절차적 노드 베이크), `preview_clownfish.png` | 없음 |
| 옐로탱 (YellowTang) | Blender 5.2.2 LTS | `assets/blender/make_yellowtang.py` (+ `fishlib.py`) | `assets/blender/YellowTang.blend`, `assets/blender/export/YellowTang.fbx`, `T_YellowTang_BaseColor.png` / `T_YellowTang_Normal.png` / `T_YellowTang_Roughness.png` (각 2048², 절차적 노드 베이크), `preview_yellowtang.png` | 없음 |
| 나비고기 (Butterflyfish) | Blender 5.2.2 LTS | `assets/blender/make_butterflyfish.py` (+ `fishlib.py`) | `assets/blender/Butterflyfish.blend`, `assets/blender/export/Butterflyfish.fbx`, `T_Butterflyfish_BaseColor.png` / `T_Butterflyfish_Normal.png` / `T_Butterflyfish_Roughness.png` (각 2048², 절차적 노드 베이크), `preview_butterflyfish.png` | 없음 |
| 자리돔 (Damselfish) | Blender 5.2.2 LTS | `assets/blender/make_damselfish.py` (+ `fishlib.py`) | `assets/blender/Damselfish.blend`, `assets/blender/export/Damselfish.fbx`, `T_Damselfish_BaseColor.png` / `T_Damselfish_Normal.png` / `T_Damselfish_Roughness.png` (각 2048², 절차적 노드 베이크), `preview_damselfish.png` | 없음 |
| 가지 산호 (BranchCoral) | Blender 5.2.2 LTS | `assets/blender/make_corals.py` (+ `fishlib.py`) | `assets/blender/BranchCoral.blend`, `assets/blender/export/BranchCoral.fbx`, `T_BranchCoral_BaseColor.png` / `T_BranchCoral_Normal.png` / `T_BranchCoral_Roughness.png` (각 1024², 절차적 노드 베이크), 공용 `preview_corals.png` | 없음 |
| 판상 산호 (PlateCoral) | Blender 5.2.2 LTS | `assets/blender/make_corals.py` (+ `fishlib.py`) | `assets/blender/PlateCoral.blend`, `assets/blender/export/PlateCoral.fbx`, `T_PlateCoral_BaseColor.png` / `T_PlateCoral_Normal.png` / `T_PlateCoral_Roughness.png` (각 1024², 절차적 노드 베이크), 공용 `preview_corals.png` | 없음 |
| 뇌 산호 (BrainCoral) | Blender 5.2.2 LTS | `assets/blender/make_corals.py` (+ `fishlib.py`) | `assets/blender/BrainCoral.blend`, `assets/blender/export/BrainCoral.fbx`, `T_BrainCoral_BaseColor.png` / `T_BrainCoral_Normal.png` / `T_BrainCoral_Roughness.png` (각 1024², 절차적 노드 베이크), 공용 `preview_corals.png` | 없음 |
| 부채 산호 (FanCoral) | Blender 5.2.2 LTS | `assets/blender/make_corals.py` (+ `fishlib.py`) | `assets/blender/FanCoral.blend`, `assets/blender/export/FanCoral.fbx`, `T_FanCoral_BaseColor.png` / `T_FanCoral_Normal.png` / `T_FanCoral_Roughness.png` (각 1024², 절차적 노드 베이크), 공용 `preview_corals.png` | 없음 |
| 관 산호 (TubeCoral) | Blender 5.2.2 LTS | `assets/blender/make_corals.py` (+ `fishlib.py`) | `assets/blender/TubeCoral.blend`, `assets/blender/export/TubeCoral.fbx`, `T_TubeCoral_BaseColor.png` / `T_TubeCoral_Normal.png` / `T_TubeCoral_Roughness.png` (각 1024², 절차적 노드 베이크), 공용 `preview_corals.png` | 없음 |

M4a 재제작(2026-09-21): 물고기 5종은 M4a에서 프로파일 곡선 로프트 몸통·실제 안구·살이 있는 막 지느러미로
다시 만들었고, 종마다 베이스 컬러·노멀·러프니스 세 장(각 2048², 절차적 노드 베이크)을 굽는다. 생성 스크립트와
산출물 경로는 위 표 그대로이며 외부 텍스처는 여전히 쓰지 않는다. 본 계약(`Root, Spine0..5, Tail, PecL, PecR`,
+X 머리, cm 단위)도 불변이다.

블루탱 비고: `assets/blender/export/preview_bluetang.png`(EEVEE 미리보기 렌더)도 스크립트 산출물이다. `.blend`/`.fbx`/`preview_bluetang.png`는 재빌드마다 바이트가 달라진다(타임스탬프·세션 데이터, EEVEE 노이즈). 베이크된 맵 3종(`T_BlueTang_{BaseColor,Normal,Roughness}.png`)은 실행마다 UV 솔기 부근 수십 서브픽셀이 최대 1/255 흔들린다(2026-09-20 측정). 재현성 검사는 `scripts/compare_texture.py`(픽셀당 최대 델타 ≤ 2/255, 달라진 픽셀 ≤ 0.01%)로 한다. Subdivision 모디파이어는 의도적으로 생략했다(shade_smooth만 적용).

산호 비고: 산호 5종은 정적 프롭이라 아마추어·애니메이션이 없다(메시 + 베이크 3장). M4b에서 3종 → 5종(부채·관 추가)으로 늘렸고, 베이스 컬러 한 장 대신 물고기와 같은 베이스컬러·노멀·러프니스 세 장(각 1024²)을 굽는다. `make_corals.py` 한 스크립트가 다섯 종을 모두 생성하며, 형상은 고정 시드(`random.Random`)와 결정론적 수식으로 재현된다. 미리보기는 다섯 산호를 x = -260 / -130 / 0 / 130 / 260에 늘어놓은 공용 `assets/blender/export/preview_corals.png` 한 장이다. 외부 에셋은 쓰지 않으며 바위 3종은 여전히 Poly Haven CC0 원본 그대로다(위 표 불변).

M4b 확인(2026-09-21): 외부 에셋 5건의 원본 파일은 M4b 브랜치에서도 한 바이트도 바뀌지 않았다
(`git diff --stat main...HEAD -- assets/models assets/textures assets/fonts` 결과 없음). 따라서 위
"도입한 외부 에셋" 표의 저작자·라이선스·수정 여부 열은 그대로 유효하다. M4b가 바위에 준 색 변화는
`MI_*` **머티리얼 인스턴스**(Unreal 쪽 에셋)로 준 것이지 원본 텍스처·메시의 수정이 아니므로
"수정: 없음"은 유지한다. 직접 제작 표는 M4b에서 `FanCoral`·`TubeCoral` 두 행이 늘었고 산호 3종의
산출물 열이 한 장에서 세 장으로 갱신되어 있으며, 위 표와 실제 `assets/blender/export/` 산출물이
일치함을 이 문서 갱신 시점에 확인했다.

## 도입 시 필수 기록

각 에셋마다 이름, 저작자, 원본 URL, 다운로드 날짜, 라이선스 URL/사본, 출처 표기 문구, 수정 여부, 게임 배포 가능 여부, 공개 저장소에 원본 재배포 가능 여부, 로컬 경로를 기록한다.

직접 제작한 경우 제작 도구, 생성 스크립트 또는 원본 파일 경로, 사용한 외부 텍스처의 출처를 기록한다. 물고기용 한국어 폰트도 같은 기준으로 관리한다.

## 조사 후보

- 바위·표면 텍스처: Poly Haven의 개별 CC0 에셋. 모래 텍스처는 `coast_sand_01`, 바위 모델은 `boulder_01`/`rock_07`/`rock_09`로 선정 완료(위 "도입한 외부 에셋" 참고).
- 물고기·산호: 적합한 무료 모델을 검토하거나 Blender로 직접 제작한다.
- 유영 애니메이션: 출처가 분명한 무료 리깅/애니메이션 또는 Blender 자체 제작.

참고: [Poly Haven 에셋 라이선스](https://polyhaven.com/license). 에셋과 사이트 콘텐츠/API 이용 조건은 구분한다.
