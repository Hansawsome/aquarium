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

M7 대기(2026-09-21): 배경 수중 앰비언스(CC0 녹음) **한 건이 아직 도입되지 않았다.** 파일이
`assets/audio/ambience/`에 놓이면 위 표에 행을 추가한다. 기대 경로·형식·라이선스 조건과 미도입 시의
실패는 `scripts/check_ambience.sh`가 말한다. 이 행이 채워지기 전에는 그 검사가 계속 빨간불이며,
그것이 이 시점의 정상 상태다. (행을 미리 채우지 않는 이유: 검사의 마지막 관문이 이 표의 행 존재를
보기 때문에, 파일 없이 행만 넣으면 아무것도 검사하지 않는 검사가 된다.)

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

M7 추가(2026-09-21) · **M8 개정(2026-09-21)**: **반응음 6종은 직접 제작이고 생성 스크립트 자체가 출처다.**
`assets/audio/make_sounds.py`(파이썬 표준 라이브러리만, numpy 없음)가
`S_Swim.wav`(1.000 s, 루프) · `S_Startle.wav`(0.130 s) · **`S_Thud.wav`(0.180 s)** · `S_Nibble.wav`(0.120 s) ·
`S_Split.wav`(0.500 s) · `S_Bubble.wav`(0.300 s)를 16비트 모노 44.1 kHz PCM으로 굽는다.

M8에서 `S_Startle`을 다시 합성했다. 개정 전에는 700→2200 Hz로 올라가는 0.220 s짜리 「꺅」이었는데,
대상 나이가 초등 5~6학년으로 바뀌면서 만화 비명 톤이 `docs/scenario.md`의 유치함 신호 목록에 올랐다.
이제 240→90 Hz로 **떨어지는** 0.130 s짜리 「퍽」이고, 잡았을 때만 나는 「쿵」(`S_Thud`)이 새로 생겼다.
주장은 귀가 아니라 측정으로 확인한다 — `assets/audio/measure_centroid.py`(표준 라이브러리 FFT)로 잰
스펙트럼 무게중심이 **S_Startle 411.5 Hz · S_Thud 364.6 Hz**이고, 개정 전 「꺅」은 같은 측정에서
**3243.6 Hz**였다.
난수는 전부 고정 시드 `random.Random`이라 **두 번 실행한 바이트가 같다**(확인 방법은 해시
리터럴이 아니라 두 번 실행 비교다). 외부 녹음은 배경 앰비언스 한 건뿐이며 아직 도입 전이다.

블루탱 비고: `assets/blender/export/preview_bluetang.png`(EEVEE 미리보기 렌더)도 스크립트 산출물이다. `.blend`/`.fbx`/`preview_bluetang.png`는 재빌드마다 바이트가 달라진다(타임스탬프·세션 데이터, EEVEE 노이즈). 베이크된 맵 3종(`T_BlueTang_{BaseColor,Normal,Roughness}.png`)은 실행마다 UV 솔기 부근 수십 서브픽셀이 최대 1/255 흔들린다(2026-09-20 측정). 재현성 검사는 `scripts/compare_texture.py`(픽셀당 최대 델타 ≤ 2/255, 달라진 픽셀 ≤ 0.01%)로 한다. Subdivision 모디파이어는 의도적으로 생략했다(shade_smooth만 적용).

산호 비고: 산호 5종은 정적 프롭이라 아마추어·애니메이션이 없다(메시 + 베이크 3장). M4b에서 3종 → 5종(부채·관 추가)으로 늘렸고, 베이스 컬러 한 장 대신 물고기와 같은 베이스컬러·노멀·러프니스 세 장(각 1024²)을 굽는다. `make_corals.py` 한 스크립트가 다섯 종을 모두 생성하며, 형상은 고정 시드(`random.Random`)와 결정론적 수식으로 재현된다. 미리보기는 다섯 산호를 x = -260 / -130 / 0 / 130 / 260에 늘어놓은 공용 `assets/blender/export/preview_corals.png` 한 장이다. 외부 에셋은 쓰지 않으며 바위 3종은 여전히 Poly Haven CC0 원본 그대로다(위 표 불변).

M4b 확인(2026-09-21): 외부 에셋 5건의 원본 파일은 M4b 브랜치에서도 한 바이트도 바뀌지 않았다
(`git diff --stat main...HEAD -- assets/models assets/textures assets/fonts` 결과 없음). 따라서 위
"도입한 외부 에셋" 표의 저작자·라이선스·수정 여부 열은 그대로 유효하다. M4b가 바위에 준 색 변화는
`MI_*` **머티리얼 인스턴스**(Unreal 쪽 에셋)로 준 것이지 원본 텍스처·메시의 수정이 아니므로
"수정: 없음"은 유지한다. 직접 제작 표는 M4b에서 `FanCoral`·`TubeCoral` 두 행이 늘었고 산호 3종의
산출물 열이 한 장에서 세 장으로 갱신되어 있으며, 위 표와 실제 `assets/blender/export/` 산출물이
일치함을 이 문서 갱신 시점에 확인했다.

M4c 확인(2026-09-21): **M4c는 에셋을 하나도 바꾸지 않았다.** 확인 방법은 가정이 아니라 측정이다 —
`git diff --stat main...HEAD -- assets/models assets/textures assets/fonts` 출력이 비어 있고, M4c 브랜치의
변경 파일은 전부 `rules/`, `unreal/Aquarium/Source/`, `unreal/Aquarium/Scripts/`, `scripts/`, `docs/`에만
있다(`.blend`/`.fbx`/`T_*.png` 산출물은 한 건도 없다). 따라서 위 "도입한 외부 에셋" 표와 "직접 제작" 표는
행·열 모두 M4b 시점 그대로 유효하다. M4c가 추가한 무리 행동은 종 구분을 새 액터 속성이 아니라
`FishMesh` **에셋 이름**에서 유도하므로 에셋 쪽 계약(종별 `SK_<종>` 이름)이 곧 종 키다 — 즉 위 표의 종 이름을
바꾸면 무리 행동의 동종 판정이 같이 바뀐다. 소품 회피 역시 프롭 메시를 바꾸지 않고 **액터 바운드**에서
원판 스택을 파생하므로 에셋 수정이 없다. 커밋된 레벨은 프롭 22개(`SCENE_OK actors=69 fish=36 props=22
tagged=22 species=5 curtains=3 tints=16`)로 M4b와 동일하다.

M5 확인(2026-09-21): **M5도 에셋을 하나도 바꾸지 않았다.** 같은 방법으로 확인했다 —
`git diff --stat main...HEAD -- assets/models assets/textures assets/fonts` 출력이 비어 있고, M5 브랜치의
변경 파일은 전부 `rules/`, `unreal/Aquarium/Source/`, `scripts/`, `docs/`에만 있다. 따라서 위 두 표는
행·열 모두 그대로 유효하다. M5의 클릭 적중 판정은 **새 충돌 에셋을 쓰지 않는다**: 적중 반경(반너비·반높이)을
`Body->CalcBounds(월드 트랜스폼)`에서 파생하므로 종별 표도, 새 물리 에셋도 생기지 않고, 플레이어
물고기의 34 cm 정규화 스케일이 자동으로 반영된다. **자동 생성된 `SK_*_PhysicsAsset`은 여전히 쓰이지
않으며, 이번에 쓰지 않기로 한 이유가 실측으로 확정되었다** — 자동 생성 캡슐이 메시보다 훨씬 뚱뚱해
(블루탱 물리 AABB 43.9 × 26.0 cm 대 메시 25.0 × 3.6 cm) 라인 트레이스를 썼다면 빈 물을 맞히고
가장 앞의 물고기도 틀리게 골랐을 것이다. M4a에서 "미사용 물리 에셋"으로 이월해 둔 항목은 이로써
**지우지 않을 이유가 아니라 지워도 되는 근거**를 하나 더 얻었다(실제 삭제 여부는 여전히 열린 항목).

M6 확인(2026-09-21) — **배포물 기준**: M4a~M5의 확인은 `git diff`로 "저장소 원본을 건드리지 않았다"까지만
말한다. M6은 다른 질문에 답한다 — **패키지 안에 무엇이 들어갔는가.**

| 검사 | 명령 | 결과 |
|---|---|---|
| 저장소 원본 불변 | `git diff --stat main...HEAD -- assets/models assets/textures assets/fonts` | 출력 없음 |
| 배포물 에셋 목록 | `UnrealPak Aquarium-Mac.utoc -List` (IoStore 컨테이너에서 정상 동작) | 2214개 청크, 고유 경로 777개 |
| 위 표 에셋 이름이 배포물에 존재 | 목록 grep | 물고기 5종 각 10건, 산호 5종 11~12건, 바위 3종 12~13건, `NotoSansKR` 1건 — **모두 존재** |
| 출처 미기재 외부 에셋 | 목록의 `/Aquarium/Content/` 항목 전수 확인 | 0건 (Fish/Props/Env/UI/Maps + 셰이더 아카이브뿐) |
| **OFL 1.1 사본 동봉** | `verify_package.sh`의 `font-licence` | **M6에서 처음 닫았다** — `Content/Licenses/OFL-NotoSansKR.txt`를 NonUFS로 스테이징한다 |

**이름 대조의 한 가지 주의.** Poly Haven 원본 이름 `coast_sand_01`은 배포물 목록에 **0건**이다. 결함이 아니라
명명 규약이다 — 모래 텍스처는 프로젝트에서 `Env/T_Sand_D|N|R`로 임포트되고 원본 파일명이 따라오지 않는다.
배포물에는 그 세 장이 `.uasset` + `.ubulk`로 정확히 들어가 있다. 바위 3종은 반대로 원본 이름
(`boulder_01`/`rock_07`/`rock_09`)을 그대로 쓰므로 목록에 그대로 보인다.

**OFL 항목은 M6에서 처음 걸린 실질적 의무다.** 이 문서는 "OFL: 임베드·재배포 허용"을 옳게 적었지만
OFL 1.1 2절은 사본 동반도 요구하고, 그 전까지 패키지에는 아무것도 들어가지 않았다. 검사는 수정 전
**Development·Shipping 양쪽 모두 FAIL**, 재패키징 후 양쪽 모두 `ok`였다 — 즉 이 검사는 무는 검사다.

M6은 에셋을 새로 만들거나 바꾸지 않았다. 따라서 위 두 표는 행·열 모두 M5 시점 그대로 유효하다.

## 도입 시 필수 기록

각 에셋마다 이름, 저작자, 원본 URL, 다운로드 날짜, 라이선스 URL/사본, 출처 표기 문구, 수정 여부, 게임 배포 가능 여부, 공개 저장소에 원본 재배포 가능 여부, 로컬 경로를 기록한다.

직접 제작한 경우 제작 도구, 생성 스크립트 또는 원본 파일 경로, 사용한 외부 텍스처의 출처를 기록한다. 물고기용 한국어 폰트도 같은 기준으로 관리한다.

## 조사 후보

- 바위·표면 텍스처: Poly Haven의 개별 CC0 에셋. 모래 텍스처는 `coast_sand_01`, 바위 모델은 `boulder_01`/`rock_07`/`rock_09`로 선정 완료(위 "도입한 외부 에셋" 참고).
- 물고기·산호: 적합한 무료 모델을 검토하거나 Blender로 직접 제작한다.
- 유영 애니메이션: 출처가 분명한 무료 리깅/애니메이션 또는 Blender 자체 제작.

참고: [Poly Haven 에셋 라이선스](https://polyhaven.com/license). 에셋과 사이트 콘텐츠/API 이용 조건은 구분한다.
