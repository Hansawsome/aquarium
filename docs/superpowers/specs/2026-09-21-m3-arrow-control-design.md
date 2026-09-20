# M3 설계 — 방향키 조종, 감속, 경계, 포커스

작성일: 2026-09-21 · 상태: 사용자 위임(2026-09-20 "다음 플랜도 같은 방식으로 결정해서 진행"), 결정은 이 문서에 기록

## 목표

입장한 아이가 방향키로 자기 물고기를 화면 기준 상하좌우로 움직인다. 키를 놓으면 부드럽게 멈추고, 창 포커스를 잃으면 멈췄다가 복귀해도 순간이동하지 않으며, 유영 영역을 벗어나지 않는다. SRS F-05, F-06, F-07의 Unreal 계층을 구현한다. 규칙 계층은 이미 있고 테스트돼 있다(`SteeringVector`, `StepMotion`, `AvoidBoundary`/`ClampToArea`).

## 결정 사항 (아쿠부장 판단, 2026-09-21)

| 항목 | 결정 | 이유 |
|---|---|---|
| 입력 방식 | `InputComponent->BindKey(EKeys::Up/Down/Left/Right, IE_Pressed/IE_Released, ...)`로 눌림 플래그를 유지하고, 매 틱 `aquarium::KeyState`를 만들어 `SteeringVector`에 넘긴다 | 프로젝트는 Enhanced Input이 기본이라 레거시 ActionMapping이 무시된다(M2 Esc에서 확인). `BindKey`는 에셋 없이 동작하고 이미 검증됐다. Enhanced Input 에셋(IMC/IA)은 Python 생성 경로가 불확실해 재현 원칙과 충돌한다 |
| 제어 주체 | `AFishActor`에 `bPlayerControlled`와 `SetInputDirection(Vec2)`를 두고, 참이면 Wander 대신 입력 방향을 쓴다 | 규칙 계층 흐름(Wander → Avoid → Motion)에서 첫 단계만 갈아끼우면 되고, 배경 물고기는 손대지 않는다 |
| 포커스 상실 | `FSlateApplication::OnApplicationActivationStateChanged`에 붙어 비활성 시 키 플래그를 모두 지우고 `MotionState.paused = true`, 복귀 시 해제 | F-06의 "입력 해제 + 일시정지 + 복귀 시 순간이동 금지". dt 상한(`maxDeltaTime` 0.1초)은 이미 규칙 계층에 있다 |
| 경계 처리 | `AvoidBoundary`(바깥 성분 제거)를 **벽을 따라 미끄러지는** `SteerAlongBoundary`로 대체한다. 규칙 계층에 새 순수 함수로 추가하고 기존 함수는 남긴다 | M1·M2b에서 기록한 후속 항목: 바깥 성분을 0으로 만들면 속도가 0을 지나 반전해 방향이 뒤집히고 수직 롤이 생긴다. 벽을 따라 조향하면 플레이어도 "벽에 붙어 미끄러지는" 자연스러운 조작감을 얻는다 |
| 안전장치 | `ClampToArea`는 그대로 유지 | 최종 이탈 방지 |
| 화면 비율 | 유영 평면은 월드 고정(4×2 m 상당)이라 화면 비율이 바뀌어도 물고기는 영역 안에 있다. 비율이 좁아지면 가장자리가 화면 밖으로 나갈 수 있으므로, 카메라 FOV 기준으로 평면 반폭을 런타임에 줄이는 보정을 게임 모드에 넣는다 | SRS F-07 "여러 화면 비율에서 이탈 없음" |
| 조작 속도 | 플레이어 물고기는 배경보다 빠르게: `MaxSpeed` 90 cm/s, `Accel` 140, `Decel` 180 (배경은 25~55) | 아이가 "반응한다"고 느낄 최소치. 방향 전환은 기존 `MaxFacingTurnRate` 슬루가 부드럽게 만든다 |

검토한 대안: Enhanced Input 에셋(재현성 문제), `IsInputKeyDown` 폴링(포커스 상실 시 잔류 상태를 직접 관리해야 함), 물고기를 Pawn으로 바꿔 `APlayerController::Possess`(P-06 고정 카메라와 충돌, 대규모 변경).

## 구성 요소

### 규칙 계층 (신규)
`rules/include/aquarium/Bounds.h`에 추가:

```cpp
// Slides along the wall instead of stopping: removes only the outward component's
// projection, so a direction that pushes into a wall becomes a direction along it.
Vec2 SteerAlongBoundary(Vec2 pos, Vec2 dir, Rect area, float avoidDistance);
```

동작: 각 축에 대해 벽 접근 거리 안이면서 바깥으로 향하면 그 축 성분을 제거하고, 남은 성분을 **원래 크기로 재정규화**한다(둘 다 막히면 영역 중심 쪽을 향한다). `AvoidBoundary`는 기존 테스트와 함께 남겨 둔다.

### `AFishActor`
- `UPROPERTY(VisibleAnywhere) bool bPlayerControlled = false;`
- `void SetInputDirection(const FVector2D& Dir)` — 화면 기준(x=오른쪽, y=위) 단위 벡터 이하.
- `void SetPaused(bool bPaused)` — `Motion.paused`를 설정.
- `StepSwim`: `bPlayerControlled`면 `Wander` 대신 입력 방향을 쓰고, `SteerAlongBoundary`를 거쳐 `StepMotion`으로 간다. 나머지(변환·본 파동·슬루)는 그대로.

### `ADiverPlayerController`
- `SetupInputComponent`: 네 방향키를 `IE_Pressed`/`IE_Released`에 묶어 `FKeyFlags{bUp,bDown,bLeft,bRight}`를 갱신.
- `Tick`: 세션이 살아 있고 플레이어 물고기가 있으면 `aquarium::SteeringVector({up,down,left,right})` 결과를 `SetInputDirection`으로 전달.
- 포커스: `FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(...)` — 비활성 시 플래그 초기화 + `SetPaused(true)`, 활성 시 `SetPaused(false)`. `EndPlay`에서 델리게이트 해제.
- 개발 전용 `-AquariumAutoInput=<패턴>`: 캡처용으로 방향키 입력을 재생한다. 패턴은 `R2,U1,L2,D1` 같은 `<방향><초>` 목록이며 반복된다. `#if !UE_BUILD_SHIPPING`.

### `AAquariumGameMode`
- 플레이어 물고기 스폰 시 `bPlayerControlled = true`, 속도 프로퍼티(90/140/180)를 적용.
- 화면 비율 보정: 뷰포트 종횡비와 카메라 FOV로 평면 거리에서 보이는 반폭을 계산해 `PlaneHalfWidth`를 그보다 크지 않게 줄인다(세로도 동일).

## 흐름

```
매 틱: 키 플래그 → SteeringVector → (플레이어 물고기) SetInputDirection
FishActor::StepSwim → dir = bPlayerControlled ? InputDir : Wander
                    → SteerAlongBoundary → StepMotion(감속 포함) → ClampToArea
                    → 위치·방향·본 파동
창 비활성 → 키 플래그 초기화 + paused=true (StepMotion이 즉시 반환)
창 활성   → paused=false (다음 틱 dt는 maxDeltaTime으로 상한)
```

## 검증

| 계층 | 검증 |
|---|---|
| 규칙 | `SteerAlongBoundary` 순수 테스트: 열린 곳에서 방향 불변, 한 벽에서 벽을 따라 미끄러짐(크기 유지), 모서리에서 안쪽을 향함, 안쪽 이동은 막지 않음. 기존 60개 유지 |
| Unreal Automation | 방향키 플래그 → 입력 벡터 매핑(대각선 정규화·반대키 상쇄), 플레이어 물고기가 입력 방향으로 이동하고 키를 놓으면 감속해 멈춤, 포커스 상실 시 위치 불변·플래그 초기화, 복귀 후 큰 dt에서도 순간이동 없음, 배경 물고기는 입력에 반응하지 않음, 여러 종횡비(16:9·4:3·21:9)에서 평면이 화면 안에 들어옴 |
| 시각 | 개발 전용 입력 재생으로 20초 영상: 상하좌우·대각선 이동, 벽에서 미끄러짐, 키를 놓았을 때 감속. 스틸 2장 |

## 데이터·오류 처리

- 세션이 없으면 방향키는 아무 일도 하지 않는다(입장 화면에서 물고기가 없으므로).
- 플레이어 물고기가 파괴된 뒤 들어온 입력은 무시한다(널 체크).
- 포커스 델리게이트는 `EndPlay`에서 반드시 해제한다.
- 별명은 여전히 로그에 남기지 않는다.

## 범위 밖

클릭 도망(M5), 실사 반복·무리 행동·소품 충돌(M4), 성능 판정·패키징(M6), 게임패드·터치 입력.
