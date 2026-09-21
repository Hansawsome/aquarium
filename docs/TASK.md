# TASK — 진행 현황

이 문서는 단계와 완료 조건을 추적하는 로드맵이다. Superpowers의 상세 TDD 구현 계획은 설계 문서 검토 후 별도로 작성한다.

## 설계 절차

- [x] 기존 폴더·GitHub 저장소·설치 환경 확인
- [x] 사용자 의도와 요구사항 논의
- [x] Unreal / 웹 접근 비교, macOS 우선 방향 합의
- [x] Superpowers 스킬 설치 및 관련 지침 읽기
- [x] PRD / SRS / 설계 / 개발 지침 초안 작성
- [x] 문서 자체 검토 — 요구사항 일관성, 8개 문서의 로컬 링크·미완성 표기 검사
- Git 커밋·푸시 결과는 실제 Git 이력과 원격 브랜치로 확인한다.
- [x] 사용자 문서 검토 — 2026-09-19 엔진(Unreal 5 유지) 확정과 함께 승인
- [x] 상세 TDD 구현 계획 작성과 실행 방식 선택 — `docs/superpowers/plans/2026-09-19-rules-layer.md`, executing-plans로 2026-09-20 실행

## 구현 로드맵 — M4b 구현 완료(사용자 시각 검토 대기)

당초 단일 단계였던 M4(실사 반복)는 2026-09-21에 **M4a / M4b / M4c 세 단계로 쪼갰다.** 한 단계에서
물고기·산호·조명·무리 행동을 동시에 바꾸면 전/후 비교에서 무엇이 좋아졌는지 판단할 수 없기 때문이다.
M4a(물고기 정밀화)와 M4b(산호·바위·조명)를 구현했고 **M4c는 열려 있다.**

| 단계 | 작업 | 완료 조건 |
|---|---|---|
| M0 | Unreal·Xcode·Blender 호환 버전 준비 | 에디터 실행과 빈 프로젝트 macOS 빌드 확인, 버전 기록 |
| M1 | 규칙 테스트 환경과 최소 수중 장면 | RED/GREEN 기록, 한 종 물고기 유영 영상, 사용자 시각 검토. **2026-09-20 구현 완료**: 규칙 60개 + Unreal Automation 9개 통과, `docs/reviews/2026-09-20-m1-swim.mp4`(34초, 1080p) 생성. **사용자 시각 검토(2026-09-20): 병합 승인, 코멘트 없음. 모델 품질 개선은 후속 반복** |
| M2 | 별명 입력·세션·무작위 배정·이름표 | F-01~04 및 F-14 테스트, 한국어 패키지 입력 확인. **2026-09-20 구현 완료**: 규칙 60 + Unreal Automation 24 통과, 클라운피시 2종째, Noto Sans KR, `docs/reviews/2026-09-20-m2-flow.mp4`(13.5초) + 스틸 3장. **사용자 검토(2026-09-20): 승인. 실제 한글 IME 타이핑·입장·이름표·나가기 사용자 직접 확인 완료** |
| M2b | 군집과 산호초 — 배경 물고기 30~40마리 무작위 유영(종·크기·시드 변화), 산호초·바위 배치 | **2026-09-20 구현 완료**: 5종 36마리 + 소품 14개(산호 3종 절차 생성, Poly Haven CC0 바위 3종), 규칙 60 + Unreal Automation 26 통과, 성능 평균 102.5 fps / p95 10.2 ms(1920×1080 에디터 빌드), `docs/reviews/2026-09-20-m2b-{reef.mp4,wide.png,school.png,perf.md}`. **사용자 시각 검토: 대기** |
| M3 | 방향키·감속·경계·자동 유영 | F-05~08 테스트, 여러 화면 비율에서 이탈 없음. **2026-09-21 구현 완료**: 규칙 66 + Unreal Automation 37 통과, 방향키 조종·포커스 상실 일시정지·속도 조절·화면 비율에 맞춘 유영 평면, `docs/reviews/2026-09-21-m3-control.mp4`(19.5초) + 스틸 2장(`-steer.png`, `-wall.png`). **사용자 시각 검토: 대기**. M1/M2b 후속 항목 "규칙 계층 벽 조향" 해소: 배경 물고기는 `SteerAlongBoundary`로 벽을 따라 미끄러져 속도 반전에 따른 방향 뒤집힘이 사라졌고, 플레이어 물고기는 `AvoidBoundary`로 벽에서 그대로 멈춘다(요청하지 않은 표류 금지). |
| M4a | 실사 반복 1 — 물고기 정밀화(프로파일 곡선 몸통·눈·막 지느러미·비늘 노멀/러프니스·서브서피스) | 개선 전/후 나란히 스틸, 근접 스틸, 성능 재측정, 사용자 판정. **2026-09-21 구현 완료**: 5종을 프로파일 곡선 로프트 몸통·실제 안구·살이 있는 막 지느러미로 다시 만들고(`verts=3275 bones=10 unweighted=0 rootMaxW=0.000`, 본 계약 `Root, Spine0..5, Tail, PecL, PecR` 불변), 종마다 2048² 3장(BaseColor/Normal/Roughness)을 베이크해 `M_<종>`을 Subsurface 셰이딩 모델로 재구성. 규칙 66 + Unreal Automation 37 통과, 평균 100.9 fps / p95 10.27 ms, `docs/reviews/2026-09-21-m4a-{closeup,scene,compare}.png` + `-reef.mp4` + `-perf.md`. **사용자 시각 검토: 대기** |
| M4b | 실사 반복 2 — 산호·바위 품질, 조명·후처리(색보정·심도·부유 입자) | 같은 방식의 전/후 비교. **2026-09-21 구현 완료**: 산호 3종 → **5종**(가지·판상·뇌·부채·관)으로 늘리고 종마다 1024² 세 장(BaseColor/Normal/Roughness)을 베이크, 프롭 14 → **22개**에 인스턴스별 요·피치/롤 ±7°·비균일 Z 스케일 0.85~1.20·틴트 머티리얼 인스턴스 21종(실사용 16종) 적용, **M1부터 다섯 단계를 살아남은 "빛줄기 약함"을 해소**(수면에 보이지 않는 그림자 전용 고보 평면 2장), 안개 재조정(`FOG_DENSITY` 5.0→2.2)으로 10 m 밖 색이 살아남, 후처리 볼륨(채도 1.15·대비 1.06·5200 K·블룸·SSAO, **심도는 명시적으로 끔**), 부유 입자(마린 스노) 반투명 커튼 3장, 스카이라이트 0.8→2.2. 규칙 66 + Unreal Automation **38** 통과, 평균 63.8 fps / p95 16.58 ms, `docs/reviews/2026-09-21-m4b-{compare,scene,coral}.png` + `-reef.mp4` + `-perf.md`. **사용자 시각 검토: 대기** |
| M4c | 무리(보이즈) 행동, 소품 충돌, 수직 전환 롤 제거 | 규칙 계층 테스트 + 영상. **열려 있음** |
| M5 | 클릭 도망·회복·애니메이션 | F-09~13 테스트, 중복 입력·중심 적중·모서리 검증 |
| M6 | macOS 시제품 품질과 배포 | SRS 성능 측정, 패키지 실행, 에셋 출처 확인 |

M0 진행(2026-09-20): Blender/CMake/Git LFS·Blender MCP 완료. Xcode 27.0 + UE 5.8.2 설치, 엔진 설정으로 호환 확인. `unreal/Aquarium/` C++ 프로젝트의 에디터 타깃 컴파일·헤드리스 에디터 실행·게임 타깃 컴파일 성공. 에디터 GUI 실행과 Unreal MCP 연결(67개 툴셋, 장면 조회, 뷰포트 캡처) 검증 완료. 남은 것: UBT 앱 마무리 단계 실패(직접 xcodebuild는 성공, SETUP.md 미해결 항목)와 패키징은 M5로 이월. **M0 종료.** 상세는 [SETUP.md](SETUP.md).

## 검증 현황

- 규칙 계층(C++17, Unreal 독립) 테스트: **66개 통과** — 2026-09-21 M4b 브랜치 `feat/m4b-reef-lighting`에서 재확인(`100% tests passed out of 66`). 최초 66개는 브랜치 `feat/m3-arrow-control` (최초 60개는 2026-09-20 `feat/m1-scene` `e56d1fe`) (`-Wall -Wextra -Wshadow` 경고 0). M1에서 SwimPlane, SwimAnimation(누적 위상), Heading 추가. 클린 빌드 `cmake -S . -B build && cmake --build build -j && ctest --test-dir build` 결과 `100% tests passed out of 66`. 각 태스크는 헤더 부재 컴파일 실패 → 스텁 assertion 실패(RED) → 구현 통과(GREEN) 순서로 진행했고 커밋 단위로 기록됨. 다룬 SRS 항목: F-01, F-02, F-03, F-05, F-06, F-07, F-08, F-10, F-11, F-12, F-14의 규칙 부분.
- Unreal Automation 테스트: **38개 Success** (`Automation RunTests Aquarium`, 헤드리스 `-nullrhi`): 규칙 링크 1, `AFishActor` 12(규칙 계층 구동·dt=0·결정성·본 존재·연쇄 본 변환·방향 연속성·본 각도 연속성·바로 선 자세·일시정지·플레이어 입력 추종·배경은 입력 무시·플레이어는 벽에서 표류 없이 정지), 게임 모드 1, 세션 9(F-02/F-03/F-14, 플레이어 물고기 크기 정규화·조종 가능·화면 비율 평면 적합·요청 기준 재적합), 이름표 3(F-04·한글 글리프·스케일 분리), UI 3(F-01 문구·분류·이중 제출 방어), 컨트롤러 8(결과→오류 매핑, 방향키 매핑, 개발 옵션 5종 파싱, 세션 전환 시 방향키 플래그 초기화). F-13의 "순간 반전 없음"은 `FacingIsContinuous`/`BoneAnglesAreContinuous`/`UpVectorStaysUpright`로 고정. 콘텐츠 1(`Aquarium.Content.PropMaterialsCompile`: `/Game/Props`·`/Game/Env`의 머티리얼 13장에서 텍스처 샘플러 51개의 샘플러 타입과 텍스처 압축 설정이 맞는지 검사 — M4a에서 두 번 당한 "Sampler type is Linear Grayscale" 사고를 그대로 잡는다. 셰이더 컴파일 오류 검사도 같이 하지만 `-nullrhi`에서는 `FMaterialResource`가 없어 항상 건너뛰므로(로그에 `0 of 13`으로 드러난다) 셰이더 컴파일 실패의 1차 방어선은 여전히 실제 게임 실행 로그 grep이다).
- **실제 렌더링 경로 머티리얼 확인(2026-09-21, M4b)**: `-nullrhi`가 아닌 **Metal RHI 실제 게임 실행**(`ReefM1 -game -benchmark -fps=30 -seconds=12`)의 로그 1575줄에서 `Failed to compile Material` / `Sampler type` / 기본 머티리얼 대체(`Default Material`, `WorldGridMaterial`) 문자열이 **0건**이다. 로그에 남은 에러는 전부 엔진·플러그인 잡음(`GameFeatureData` 규칙 부재, 엔진 실험 플러그인의 `init_unreal.py` 임포트 실패, `LogTemp`의 엔진 자체 에러 테스트)으로 프로젝트와 무관하다.
- 시각 검토(M4b): `docs/reviews/2026-09-21-m4b-{compare.png,scene.png,coral.png,reef.mp4,perf.md}` — `scripts/render_m4b_compare.sh`가 산호 5종 근접 스틸·산호초 영상·장면 스틸·나란히 비교를 한 번에 만든다. 비교 대상은 같은 맵·시드·자동 입력의 M4a 스틸(`2026-09-21-m4a-scene.png`). 제작자 확인: **빛줄기가 처음으로 빛줄기로 보인다**, 안개를 낮춰 10~20 m 배경 물고기가 종별 노랑·파랑·주황으로 읽히기 시작했고(M4a 작업이 비로소 화면에 나타났다), 프롭 22개가 같은 메시를 찍어낸 것처럼 보이지 않는다. **사용자 시각 검토: 대기**. 알려진 품질 한계: TubeCoral이 말뚝 울타리처럼 읽히고, FanCoral 실루엣이 BranchCoral과 거의 같으며, PlateCoral·BrainCoral이 흰 덩어리로 남아 있다(아래 후속 항목).
- **M4b에서 잡은 잠복 버그 4건**:
  1. **Blender 5.2의 Bump 노드 `Distance` 기본값이 1.0이 아니라 0.001** — 산호 노멀 맵이 조용히 전부 무효화되어 있었다. 더 나쁜 것은 그런 맵 하나가 분산 2.37e-06으로 **퇴화 검사를 통과했다**는 점이다. `bake_maps`에 `min_variance`를 인자로 뚫어 산호는 1e-4를 요구하도록 올렸고, 기존 바닥값이라면 통과시켰을 3.83e-5 맵에서 실제로 검사가 걸리는 것을 확인했다.
  2. **`PropMaterialsCompile` 테스트가 계획대로는 공허했다** — 38/38 Success를 보고하면서 로그에는 `checked 0 of 13 materials`를 찍고 있었다. `-nullrhi`에 `FMaterialResource`가 없기 때문이다. RHI와 무관한 샘플러 타입·압축 설정 일치 검사(샘플러 51개)로 다시 썼고, 일부러 망가뜨린 머티리얼에서 빨간불이 켜지는 것을 확인했다. 남은 한계(셰이더 컴파일 오류는 헤드리스에서 여전히 검사되지 않음)는 위에 그대로 적었다.
  3. **프롭 레인 회피가 프롭의 피벗만 검사했다** — 반경 137 cm짜리 바위가 y=65에 서서 카메라 축을 가로질러 플레이어 물고기를 가릴 수 있었다. `|y| >= lane(x) + radius + margin`으로 고쳤다. 같은 결함이 `build_reef_m1.py`와 `verify_scene.py` **양쪽에** 있었기 때문에 검증이 이 문제를 한 번도 잡지 못했다. `verify_scene.py`는 이제 표를 베껴 쓰지 않고 실제 메시 바운드에서 반경을 계산한다.
  4. **비교 하네스가 한때 M4b를 M4b와 비교했다** — 이름 변경이 `BEFORE` 기본값까지 같이 바꿔 버린 탓이다.
- 시각 검토(M4a): `docs/reviews/2026-09-21-m4a-{compare.png,scene.png,closeup.png,reef.mp4}` — `scripts/render_m4a_compare.sh`가 근접 스틸·산호초 영상·장면 스틸·나란히 비교를 한 번에 만든다. 비교 대상은 같은 맵·시드·자동 입력 패턴의 M3 스틸(`2026-09-21-m3-wall.png`, t=12). 제작자 확인: 내 물고기는 눈·갈라진 꼬리·등/뒷지느러미·짙은 무늬가 분명히 보여 확실히 나아졌고, 10~20 m 뒤 안개 속 배경 물고기는 전후 차이를 알기 어렵다. 바위는 머티리얼이 컴파일되기 시작하면서 크게 좋아졌다. **사용자 시각 검토: 대기**. 알려진 품질 한계: 비늘이 1 m에서 겹친 비늘 줄이 아니라 고운 가죽 질감으로 읽히고, 두 탱과 나비고기의 가슴지느러미가 약간 아래로 처지고 크다(게임 거리에서는 안 보임).
- **M4a에서 잡은 잠복 버그 2건(둘 다 캡처를 눈으로 보고서야 발견)**: 물고기 5종 머티리얼과 바위 3종 머티리얼(`M_boulder_01`, `M_rock_07`, `M_rock_09`)이 "Sampler type is Linear Grayscale, should be Linear Color"로 컴파일에 실패해 회색 기본 머티리얼로 그려지고 있었다. 원인은 두 번 다 같다 — sRGB를 끈 러프니스 텍스처를 `TC_Default`로 임포트한 채 머티리얼이 `SAMPLERTYPE_LINEAR_GRAYSCALE`로 샘플링. `import_fish.py`·`import_props.py`에서 `TC_MASKS` + `SAMPLERTYPE_MASKS`로 고정했다.
- 시각 검토(M3): `docs/reviews/2026-09-21-m3-{control.mp4,steer.png,wall.png}` — 개발 전용 옵션(`-AquariumAutoInput`, `-AquariumAutoNickname`, `-AquariumAssignmentSeed`, `-AquariumCaptureUI`)으로 `scripts/render_m3_video.sh`가 재현한다. 제작자 확인: 네 방향 조종, 키를 놓으면 부드러운 감속, 벽에 붙였을 때 세로 표류 없음, 이름표는 계속 머리 위. **사용자 시각 검토: 대기**. 알려진 품질 한계: 벽을 따라 붙어 있을 때 자세가 가파르게 서는 순간이 있다.
- 시각 검토(M2): `docs/reviews/2026-09-20-m2-{flow.mp4,entry.png,nametag.png,after-exit.png}` — 개발 전용 옵션(`-AquariumAutoNickname`, `-AquariumAutoExitAfter`, `-AquariumAssignmentSeed`, `-AquariumCaptureUI`)으로 재현. 제작자 확인: 한글 입장 화면, 앞쪽 큰 플레이어 물고기 위 이름표 추적, HUD 나가기, 퇴장 후 복귀. **사용자 검토 결과(2026-09-20): 승인, 한글 IME 직접 확인.** 알려진 품질 한계: 수직으로 방향을 바꿀 때 짧은 롤(0.3초), 가까운 블루탱의 짙은 남색 텍스처.
- 시각 검토(M1): `docs/reviews/2026-09-20-m1-still.png`, `docs/reviews/2026-09-20-m1-swim.mp4`. 제작자 자체 확인: 수중 안개·빛줄기·바닥 코스틱·모래·블루탱 유영이 보이고 영상 내 순간이동·반전 없음. **사용자 검토 결과(2026-09-20): 병합 승인, 코멘트 없음.** 알려진 품질 한계: 물고기 모델은 1차 실루엣(실사 아님), 빛줄기 약함, 검은 옆줄무늬가 거리 때문에 잘 안 보임.
- 미구현 Unreal 연동: F-09 레이캐스트(M5).
- F-06(포커스 상실 일시정지)은 `SetPaused` 계열 테스트로 로직을 고정했지만, 실제 윈도 포커스 델리게이트는 헤드리스에서 발생하지 않는다. 창 모드로 한 번 수동 확인(다른 앱으로 전환했다가 돌아오기)이 필요하다.
- 성능 측정: **2026-09-21 재측정(M4b 최종 출하 설정)** — 물고기 36 + 프롭 22 + 부유 입자 커튼 3 + 고보 2, 1920×1080 에디터 빌드에서 평균 **63.8 fps / p95 16.58 ms**(두 번 실행이 일치). SRS 목표(60 fps·22 ms)는 충족하지만 평균 fps 여유가 약 6 %뿐이다(M4a는 100.9 fps로 여유가 훨씬 넉넉했다). M4a 대비 평균 fps는 약 37 % 내려갔다. **항목별 토글 실측에서 비용의 사실상 전부가 볼류메트릭 안개 격자 한 항목이었다**: 격자를 엔진 기본값(`GridPixelSize=8`, `GridSizeZ=64`)으로 되돌리면 94.5 fps(+30.4)로 돌아오고, 커튼 3장 전부 제거는 +2.2, 고보·SSAO·프롭 22→14는 전부 실행 간 노이즈 바닥(**약 1.3 fps**) 안이었다. 즉 **"반투명 커튼이 위험하다"는 설계 단계 예측은 측정으로 반증되었다.** M5/M6용 예산 회수 레버는 `GridSizeZ` 128→64(+17.1 fps, 화면 XY 해상도는 유지)이며 **이번에는 일부러 적용하지 않았다** — 필요해지기 전에 화질을 깎을 이유가 없고, M6의 쿡 빌드는 이 에디터 빌드 수치보다 빠를 것이기 때문이다. 상세는 [`docs/reviews/2026-09-21-m4b-perf.md`](reviews/2026-09-21-m4b-perf.md).
- 이전 성능 측정: **2026-09-21(M4a, 커밋 `2c654e0`)** — 물고기 36 + 소품 14, 1920×1080, 에디터 빌드에서 평균 100.9 fps / p95 10.27 ms. M2b 기준선(2026-09-20, 평균 102.5 fps / p95 10.22 ms) 대비 평균 -1.6%로 측정 노이즈 수준이고 SRS 목표(60 fps·22 ms)를 크게 웃돈다. 상세는 [`docs/reviews/2026-09-21-m4a-perf.md`](reviews/2026-09-21-m4a-perf.md). 이 문서에 잠시 적혔던 102.0 fps는 바위 머티리얼이 회색 기본 머티리얼로 그려지던 상태의 값이라 폐기했다. 패키징 빌드 기준 판정은 M6.
- 내 물고기는 카메라에 가장 가까운 평면(X=220)에 두고 종에 상관없이 몸길이 34 cm로 정규화해 화면에서 가장 크게 보인다(사용자 요청, 2026-09-20).
- Unreal 빌드: 에디터·게임 타깃 컴파일 성공, 헤드리스 에디터 실행 성공 (2026-09-20, SETUP.md). 화면·성능: 미실행.
- 무료 에셋: Poly Haven `coast_sand_01` (CC0) 도입, `docs/ASSETS.md`에 출처 기록. 물고기·텍스처는 직접 제작.
- 현재 완료 범위: 설계 문서, 개발 도구·MCP 연결(M0), 규칙 계층, M1 수중 장면(사용자 승인), M2 세션·이름표(사용자 승인), M2b 군집·산호초(사용자 검토 대기), M3 방향키 조종(사용자 검토 대기), M4a 물고기 정밀화(사용자 검토 대기), M4b 산호·조명·후처리(사용자 검토 대기). **M4c(무리 행동·소품 충돌·수직 롤 제거), M5(클릭 도망), M6(패키징·성능 판정)은 아직 열려 있다.**
- M4a 후속 항목(비늘 이방성 셀, 가슴지느러미 위치, 나비고기 `pecStray=16`, 고정 바운드의 `BoundsScale`, 미사용 `SK_*_PhysicsAsset`)은 [M4a 구현 계획](superpowers/plans/2026-09-21-m4a-fish-fidelity.md)의 "구현 중 발견한 후속 항목"에 기록했다. M4b 후속 항목(TubeCoral 말뚝 울타리 실루엣, FanCoral이 BranchCoral과 구분되지 않음, PlateCoral·BrainCoral의 흰 덩어리 — `CORAL_TINTS[0]`이 항등 틴트라서이며 M4b 퇴행이 아님, 스카이라이트 상향에 따른 바닥 밝기, `GOBO_COARSE_THRESHOLD`와 빛줄기 선명도의 절충)은 [M4b 구현 계획](superpowers/plans/2026-09-21-m4b-reef-lighting.md)의 같은 절에 기록했다.
