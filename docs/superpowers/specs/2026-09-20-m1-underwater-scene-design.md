# M1 설계 — 최소 수중 장면과 블루탱 한 마리의 유영

작성일: 2026-09-20 · 상태: 사용자 승인(대화), 문서 검토 대기

## 목표

Unreal 5.8 프로젝트(`unreal/Aquarium`)에서 고정 잠수부 시점의 수중 장면 안에 Blender로 직접 제작한 블루탱 한 마리가 규칙 계층(`aquarium_rules`)의 자율 유영으로 움직이는 30초 이상 영상을 만들어 사용자의 첫 시각 검토를 받는다. 이 단계에서 "실사에 가까운가"를 처음 판단한다.

## 결정 사항 (2026-09-20 논의)

| 항목 | 결정 | 이유 |
|---|---|---|
| 에셋 조달 | Blender 직접 제작 (Blender MCP 사용) | 라이선스 부담 없음, 완전 통제 |
| 종 | 블루탱 (*Paracanthurus hepatus*) | 납작한 타원 몸통, 강한 파랑·노랑 대비로 실사 판단이 쉬움 |
| 장면 범위 | 수중 분위기만 — 산호 없음 | 판단을 물고기 유영 품질에 집중 |
| 이동 구동 | 규칙 계층 `WanderBehavior` + `AvoidBoundary` + `StepMotion` 연동 | "실제 앱이 같은 규칙 코드를 사용" 원칙을 초기에 검증 |
| 애니메이션 | 뼈대 리깅 + 절차적 사인파. 계산은 규칙 계층의 순수 함수 `SwimAnimation`(속도·시간 → 본 각도)이 하고, `UPoseableMeshComponent`가 본에 적용한다. Animation Blueprint는 쓰지 않는다 (계획 작성 시 변경: 테스트 가능성과 스크립트 재현성) | F-13(속도 연동 진폭·주기)을 파라미터로 구현, 이후 도망 동작으로 확장 |

검토한 대안: 무료 리깅 모델(빠르지만 라이선스·수정 제약), Fab 무료 에셋(공개 저장소 재배포 불가), 키프레임 유영 루프(속도 연동 불리), 머티리얼 WPO 흔들림(몸통·지느러미 분리 제어 어려움).

## 구성 요소

### Blender — `assets/blender/BlueTang.blend` (Git LFS로 추적)
- 메시: 좌우 대칭 타원 몸통, 등·뒷·꼬리·가슴 지느러미. 목표 삼각형 수 8k~15k.
- 리깅: 척추 본 6개(머리→꼬리), 꼬리지느러미 본 1개, 가슴지느러미 본 좌우 1개씩. 루트 본은 원점.
- 재질: 직접 페인트한 베이스 컬러(파랑 몸통, 검은 팔레트 무늬, 노란 꼬리), 러프니스, 노멀. 텍스처 2048².
- 내보내기: FBX (Z-up→Unreal 변환 옵션, 스케일 1 = 1cm 기준, 실제 몸길이 약 25cm).
- 제작 기록: 사용한 스크립트 또는 단계와 원본 경로를 `docs/ASSETS.md`에 남긴다. 외부 텍스처를 쓰면 출처를 기록한다.

### Unreal — `unreal/Aquarium`
- `Content/Fish/BlueTang/`: 스켈레탈 메시, 머티리얼. 에셋 생성은 `unreal/Aquarium/Scripts/*.py`(에디터 Python)로 재현 가능하게 한다.
- `Content/Maps/ReefM1.umap`: 수중 장면. `DefaultEngine.ini`의 시작 맵을 이 맵으로 교체.
- `Source/Aquarium/`:
  - `Aquarium.Build.cs`가 저장소 루트의 `rules/include`를 include 경로에, `rules/src/*.cpp`를 소스에 포함한다(복제 금지).
  - `AFishActor`: 스켈레탈 메시 컴포넌트를 갖고, 매 틱 규칙 계층을 호출해 위치·회전을 갱신한다. 유영 평면(2D `Vec2`)을 월드 좌표로 바꾸는 변환은 이 액터의 책임이다.
  - 애니메이션: `AFishActor`가 매 틱 `aquarium::SwimAnimation::BoneAngles(speed, turnRate, phase)` (위상은 `AdvancePhase`로 매 틱 누적)를 호출해 얻은 각도를 `UPoseableMeshComponent::SetBoneRotationByName`으로 척추·꼬리 본에 적용한다.

### 좌표 변환
규칙 계층은 x+ = 화면 오른쪽, y+ = 화면 위인 2D 평면을 쓴다. `AFishActor`는 유영 평면의 원점·가로축·세로축(월드 벡터)을 프로퍼티로 갖고, `Vec2 → FVector = Origin + x·Right + y·Up`으로 변환한다. 물고기의 진행 방향은 속도 벡터를 같은 방식으로 변환해 `LookAt` 회전으로 만든다. 깊이 방향은 M1에서 고정한다.

## 프레임 흐름

```
Tick(dt)
 → wander.Update(pos2D, dt)
 → dir = AvoidBoundary(pos2D, wander.DesiredDirection(pos2D), area, avoid)
 → StepMotion(motion, dir, params, dt)
 → motion.position = ClampToArea(motion.position, area)
 → SetActorLocation(ToWorld(motion.position)), SetActorRotation(LookAt(ToWorld(velocity)))
 → angles = SwimAnimation::BoneAngles(|velocity|, Δyaw/dt, phase) → PoseableMesh.SetBoneRotationByName(...)
```

## 장면

- 카메라: 고정 `CameraActor`, 잠수부 눈높이. 흔들림 없음.
- 조명: 지향성 라이트 1개(수면 방향, 볼류메트릭 산란 활성), 스카이라이트 약하게.
- 물: 지수 높이 안개(푸른 청록, 밀도 높게) + 볼류메트릭 안개로 빛줄기.
- 바닥: 평면 + Poly Haven CC0 모래 텍스처(사용 파일과 URL을 `docs/ASSETS.md`에 기록).
- 코스틱: Light Function 머티리얼로 절차적 패닝 무늬.
- 부유 입자: M1에서 제외, M5 품질 조정으로 이월 (계획 작성 시 변경: Niagara를 스크립트로 재현 가능하게 만들 확실한 경로가 없음).
- 유영 영역: 카메라 앞 3.3 m 지점, 가로 4 m × 세로 2 m 평면, 경계 회피 거리 0.5 m (당초 6×3 m였으나 75° FOV에 들어오도록 2026-09-20 축소).

## 검증

| 계층 | 검증 |
|---|---|
| 규칙 계층 | 기존 42개 테스트 유지 (`ctest`). 필요 시 좌표 변환용 순수 함수를 추가하면 같은 방식으로 테스트 |
| Unreal 연동 | Automation Test 2개: (1) `AFishActor` 한 틱 후 위치가 규칙 계층 결과와 일치 (2) dt=0에서 위치 불변. 에디터 commandlet `-ExecCmds="Automation RunTests Aquarium"`으로 실행하고 로그를 기록 |
| 시각 | 30초 이상 유영 영상. Movie Render Queue 또는 MCP `CaptureViewport` 연속 캡처를 ffmpeg로 합침. 사용자 검토 결과와 코멘트를 `docs/TASK.md`에 기록 |
| 성능 | M1에서는 측정하지 않는다 (M5) |

## 오류·제약 처리

- FBX 임포트 후 본 수·스케일이 기대와 다르면 Blender 내보내기 설정을 고치고 재시도한다. 임포트 결과(본 수, 바운드)를 로그로 확인한다.
- 규칙 계층은 표준 C++17만 쓴다. Unreal 빌드에서 경고가 나면 `Build.cs`의 해당 소스에만 경고 수준을 조정하고, 규칙 코드를 Unreal용으로 복제하거나 `#ifdef`로 갈라놓지 않는다. 규칙 변경이 필요하면 순수 C++ 테스트를 먼저 고친다.
- Unreal MCP·Blender MCP는 개발 도구다. 게임 실행에 필요하지 않다.

## 범위 밖

산호·바위, 두 번째 종, 별명 입력·세션·이름표(M2), 방향키 제어(M3), 클릭 도망(M4), 성능 측정·패키징(M5).
