# M3 — 방향키 조종 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 입장한 플레이어가 방향키로 자기 물고기를 화면 기준으로 움직이고, 키를 놓으면 부드럽게 멈추며, 벽에서는 미끄러지고, 창 포커스를 잃으면 멈춘다 (SRS F-05, F-06, F-07).

**Architecture:** 규칙 계층에 벽을 따라 조향하는 순수 함수를 추가한다. `AFishActor`는 `bPlayerControlled`일 때 Wander 대신 주입된 입력 방향을 쓴다. `ADiverPlayerController`가 방향키를 `BindKey`로 받아 플래그를 유지하고 매 틱 `SteeringVector` 결과를 물고기에 넘기며, Slate 활성 상태 델리게이트로 일시정지를 건다. 게임 모드는 플레이어 물고기에 조작용 속도와 화면 비율 보정을 적용한다.

**Tech Stack:** C++17 규칙 계층 + Catch2, Unreal 5.8.2 C++ (Automation), ffmpeg.

**공통 명령** (저장소 루트):
- ctest: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure`
- UE 빌드(끝에 두 번): `"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex 2>&1 | grep -vi "\[upgrade\]" | grep -E "error|warning: |Result"`
- UE 테스트: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "Test Completed|LogAutomationController: Error"` (현재 27개)

**파일 구조(변경분):**
```
rules/include/aquarium/Bounds.h        # + SteerAlongBoundary
tests/test_bounds.cpp                  # + 4 케이스
unreal/Aquarium/Source/Aquarium/
  FishActor.h/.cpp                     # bPlayerControlled, SetInputDirection, SetPaused
  DiverPlayerController.h/.cpp         # 방향키 BindKey, 포커스 델리게이트, -AquariumAutoInput
  AquariumGameMode.h/.cpp              # 조작 속도, 화면 비율 보정
  Tests/{FishActorTests,ControllerTests,SessionTests}.cpp
scripts/render_m3_video.sh
docs/reviews/<날짜>-m3-{control.mp4,steer.png,wall.png}
```

---

### Task 0: 브랜치

- [ ] **Step 1**

```bash
git checkout main && git pull && git checkout -b feat/m3-arrow-control
```

---

### Task 1: `SteerAlongBoundary` (규칙 계층, F-07)

**Files:** Modify `rules/include/aquarium/Bounds.h`, `tests/test_bounds.cpp`

- [ ] **Step 1: 실패하는 테스트** — `tests/test_bounds.cpp`에 추가

```cpp
TEST_CASE("steer along boundary keeps direction in open water") {
    Vec2 d = SteerAlongBoundary({50.f, 50.f}, Vec2{1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == Approx(1.f));
    REQUIRE(d.y == Approx(0.f));
}

TEST_CASE("steer along boundary slides along a wall at full magnitude") {
    // Heading up-right into the right wall: the outward x is dropped and the
    // remaining +y is renormalized, so the fish slides up the wall at full speed.
    const Vec2 In = Vec2{1.f, 1.f}.Normalized();
    Vec2 d = SteerAlongBoundary({95.f, 50.f}, In, kArea, 10.f);
    REQUIRE(d.x == 0.f);
    REQUIRE(d.y == Approx(1.f));
    REQUIRE(d.Length() == Approx(1.f));
}

TEST_CASE("steer along boundary turns inward in a corner") {
    // Both axes blocked in the top-right corner: aim back at the area centre.
    Vec2 d = SteerAlongBoundary({95.f, 95.f}, Vec2{1.f, 1.f}.Normalized(), kArea, 10.f);
    REQUIRE(d.x < 0.f);
    REQUIRE(d.y < 0.f);
    REQUIRE(d.Length() == Approx(1.f));
}

TEST_CASE("steer along boundary does not block inward movement") {
    Vec2 d = SteerAlongBoundary({95.f, 50.f}, Vec2{-1.f, 0.f}, kArea, 10.f);
    REQUIRE(d.x == Approx(-1.f));
}

TEST_CASE("steer along boundary passes a zero direction through") {
    Vec2 d = SteerAlongBoundary({95.f, 95.f}, Vec2{0.f, 0.f}, kArea, 10.f);
    REQUIRE(d.Length() == 0.f);
}
```

- [ ] **Step 2: 실패 확인** — ctest 명령. Expected: `'SteerAlongBoundary' was not declared`.

- [ ] **Step 3: 구현** — `Bounds.h`에 추가(기존 `AvoidBoundary`는 그대로 둔다)

```cpp
// Slides along the wall instead of stopping. Drops the outward component of `dir` on each
// blocked axis and renormalizes what is left to the original magnitude, so a fish pushed into
// a wall keeps moving along it instead of decelerating to zero and reversing (which flipped
// the facing by 180 degrees; see the M1 review). With both axes blocked it aims back at the
// centre of the area. A zero direction passes through unchanged.
inline Vec2 SteerAlongBoundary(Vec2 pos, Vec2 dir, Rect area, float avoidDistance) {
    const float len = dir.Length();
    if (len <= 1e-6f) return dir;

    const bool blockedX = (dir.x > 0.f && pos.x > area.maxX - avoidDistance) ||
                          (dir.x < 0.f && pos.x < area.minX + avoidDistance);
    const bool blockedY = (dir.y > 0.f && pos.y > area.maxY - avoidDistance) ||
                          (dir.y < 0.f && pos.y < area.minY + avoidDistance);
    if (!blockedX && !blockedY) return dir;

    if (blockedX && blockedY) {
        const Vec2 centre{(area.minX + area.maxX) * 0.5f, (area.minY + area.maxY) * 0.5f};
        const Vec2 inward = (centre - pos).Normalized();
        return inward.Length() > 0.f ? inward * len : dir;
    }
    Vec2 along{blockedX ? 0.f : dir.x, blockedY ? 0.f : dir.y};
    const Vec2 unit = along.Normalized();
    return unit.Length() > 0.f ? unit * len : dir;
}
```

- [ ] **Step 4: 통과 확인** — ctest 명령. Expected: `100% tests passed out of 65`.

- [ ] **Step 5: 커밋**

```bash
git add rules/include/aquarium/Bounds.h tests/test_bounds.cpp
git commit -m "feat: SteerAlongBoundary slides along walls instead of stalling (F-07)"
```

---

### Task 2: `AFishActor` 플레이어 조종 모드

**Files:** Modify `Source/Aquarium/FishActor.h/.cpp`, `Source/Aquarium/Tests/FishActorTests.cpp`

- [ ] **Step 1: 실패하는 테스트** — `FishActorTests.cpp`에 추가(기존 `SpawnFish(World, Seed)` 헬퍼 사용)

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFollowsInput, "Aquarium.Fish.PlayerControlledFollowsInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFollowsInput::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->bPlayerControlled = true;
	Fish->SetInputDirection(FVector2D(1.f, 0.f));          // screen right
	const FVector Before = Fish->GetActorLocation();
	for (int i = 0; i < 20; ++i) Fish->StepSwim(0.05f);
	const FVector After = Fish->GetActorLocation();
	TestTrue(TEXT("moved along +Y (screen right)"), After.Y - Before.Y > 5.f);
	TestTrue(TEXT("did not drift vertically"), FMath::Abs(After.Z - Before.Z) < 2.f);

	// Releasing the keys decelerates to a stop instead of continuing (F-06).
	Fish->SetInputDirection(FVector2D::ZeroVector);
	for (int i = 0; i < 40; ++i) Fish->StepSwim(0.05f);
	TestTrue(TEXT("stopped after release"), Fish->CurrentSpeed() < 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorPauseFreezes, "Aquarium.Fish.PausedDoesNotMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorPauseFreezes::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->bPlayerControlled = true;
	Fish->SetInputDirection(FVector2D(1.f, 0.f));
	for (int i = 0; i < 10; ++i) Fish->StepSwim(0.05f);
	Fish->SetPaused(true);
	const FVector Frozen = Fish->GetActorLocation();
	for (int i = 0; i < 20; ++i) Fish->StepSwim(0.05f);
	TestTrue(TEXT("no movement while paused"), Fish->GetActorLocation().Equals(Frozen, 1e-3f));
	// A long frame after resuming must not teleport (dt is clamped by MotionParams).
	Fish->SetPaused(false);
	Fish->StepSwim(5.0f);
	TestTrue(TEXT("no teleport on resume"), FVector::Dist(Fish->GetActorLocation(), Frozen) < 60.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorBackgroundIgnoresInput, "Aquarium.Fish.BackgroundIgnoresInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorBackgroundIgnoresInput::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* A = SpawnFish(World, 42u);
	AFishActor* B = SpawnFish(World, 42u);
	B->SetInputDirection(FVector2D(1.f, 0.f));             // not player-controlled: ignored
	for (int i = 0; i < 40; ++i) { A->StepSwim(0.05f); B->StepSwim(0.05f); }
	TestTrue(TEXT("same wander path"), A->GetActorLocation().Equals(B->GetActorLocation(), 1e-3f));
	return true;
}
```

- [ ] **Step 2: 실패 확인** — UE 빌드. Expected: `no member named 'bPlayerControlled'`.

- [ ] **Step 3: 구현** — `FishActor.h`에 추가:

```cpp
	// Player-controlled fish take their direction from input instead of the wander behaviour.
	UPROPERTY(VisibleAnywhere, Category = "Swim") bool bPlayerControlled = false;
	// Screen-relative unit direction (x = right, y = up); zero means "no keys held".
	void SetInputDirection(const FVector2D& Dir);
	// Focus loss pauses the simulation (F-06); StepSwim returns immediately while paused.
	void SetPaused(bool bPaused);
	bool IsPaused() const { return Motion.paused; }
```
`.cpp`: `SetInputDirection`은 `InputDir = Dir.GetSafeNormal()`(길이가 1을 넘지 않게), `SetPaused`는 `Motion.paused = bPaused`. `StepSwim`의 방향 계산을

```cpp
	aquarium::Vec2 Desired;
	if (bPlayerControlled)
	{
		Desired = aquarium::Vec2{static_cast<float>(InputDir.X), static_cast<float>(InputDir.Y)};
	}
	else
	{
		Wander->Update(Motion.position, DeltaSeconds);
		Desired = Wander->DesiredDirection(Motion.position);
	}
	const aquarium::Vec2 Dir = aquarium::SteerAlongBoundary(Motion.position, Desired, Area, AvoidDistance);
```
로 바꾼다(기존 `AvoidBoundary` 호출을 대체). `StepSwim` 맨 앞의 `DeltaSeconds <= 0.f` 조기 반환은 유지하고, `Motion.paused`는 `StepMotion`이 이미 처리하지만 본 파동·회전도 멈추도록 `if (Motion.paused) return;`를 추가한다.

- [ ] **Step 4: 통과 확인** — UE 빌드 두 번 → 테스트. Expected: 30개 Success(27 + 3).

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Source
git commit -m "feat: player-controlled fish take input direction and can be paused (F-05, F-06)"
```

---

### Task 3: 컨트롤러 입력과 포커스

**Files:** Modify `Source/Aquarium/DiverPlayerController.h/.cpp`, `Source/Aquarium/Tests/ControllerTests.cpp`

- [ ] **Step 1: 실패하는 테스트** — 순수 로직만 검사. 컨트롤러에 정적 헬퍼를 두고 그것을 테스트한다.

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerMapsKeysToDirection, "Aquarium.Controller.MapsArrowKeysToDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControllerMapsKeysToDirection::RunTest(const FString&)
{
	using FKeys = ADiverPlayerController::FArrowKeys;
	TestEqual(TEXT("right"), ADiverPlayerController::DirectionFor(FKeys{false, false, false, true}), FVector2D(1.f, 0.f));
	TestEqual(TEXT("up"), ADiverPlayerController::DirectionFor(FKeys{true, false, false, false}), FVector2D(0.f, 1.f));
	TestEqual(TEXT("opposing keys cancel"), ADiverPlayerController::DirectionFor(FKeys{true, true, true, true}), FVector2D::ZeroVector);
	const FVector2D Diag = ADiverPlayerController::DirectionFor(FKeys{true, false, false, true});
	TestTrue(TEXT("diagonal normalized"), FMath::IsNearlyEqual(static_cast<float>(Diag.Size()), 1.f, 1e-4f));
	TestTrue(TEXT("diagonal points up-right"), Diag.X > 0.f && Diag.Y > 0.f);
	return true;
}
```

- [ ] **Step 2: 실패 확인** — UE 빌드. Expected: `no type named 'FArrowKeys'`.

- [ ] **Step 3: 구현**
  - 헤더에 `struct FArrowKeys { bool bUp = false, bDown = false, bLeft = false, bRight = false; };`와 `static FVector2D DirectionFor(const FArrowKeys&);`(내부에서 `aquarium::SteeringVector` 호출), 멤버 `FArrowKeys ArrowKeys;`.
  - `SetupInputComponent`에 여덟 개의 `BindKey`(`EKeys::Up/Down/Left/Right` × `IE_Pressed`/`IE_Released`)를 추가해 플래그를 갱신한다. 람다 대신 `UFUNCTION`이 아닌 멤버 함수 8개 또는 `BindKey(...).Delegate` 형태로, 기존 Esc 바인딩 옆에 둔다.
  - `Tick`에 세션이 활성이고 `GameMode()->PlayerFish()`가 유효하면 `PlayerFish()->SetInputDirection(DirectionFor(ArrowKeys))`.
  - `BeginPlay`에서 `FSlateApplication::IsInitialized()`이면 `OnApplicationActivationStateChanged().AddUObject(this, &ADiverPlayerController::HandleAppActivationChanged)`를 등록하고 핸들을 저장, `EndPlay`에서 `Remove`. 핸들러는 비활성 시 `ArrowKeys = {}` + 플레이어 물고기 `SetPaused(true)`, 활성 시 `SetPaused(false)`.
  - 개발 전용 `-AquariumAutoInput=<패턴>`(`#if !UE_BUILD_SHIPPING`): `static bool ParseAutoInput(const TCHAR*, FString&)`로 파싱하고, `R2,U1,L2,D1` 형식(`R/L/U/D/0` + 초)을 순환 재생해 `ArrowKeys`를 덮어쓴다. `ParseAutoInput`도 테스트한다(빈 값·없음·nullptr → false, 정상 문자열 → true).

- [ ] **Step 4: 통과 확인** — UE 빌드 두 번 → 테스트. Expected: 32개 Success(30 + 방향 매핑 + AutoInput 파서).

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Source
git commit -m "feat: arrow-key control, focus pause and dev input playback (F-05, F-06)"
```

---

### Task 4: 게임 모드 — 조작 속도와 화면 비율 보정

**Files:** Modify `Source/Aquarium/AquariumGameMode.h/.cpp`, `Source/Aquarium/Tests/SessionTests.cpp`

- [ ] **Step 1: 실패하는 테스트**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionPlayerFishIsControllable, "Aquarium.Session.PlayerFishIsControllable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSessionPlayerFishIsControllable::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = SpawnGameMode(World, 1, 2);
	GM->BeginSession(TEXT("니모"));
	AFishActor* Fish = GM->PlayerFish();
	if (!TestNotNull(TEXT("player fish"), Fish)) return false;
	TestTrue(TEXT("player controlled"), Fish->bPlayerControlled);
	TestTrue(TEXT("faster than background"), Fish->MaxSpeed >= 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionPlaneFitsAspect, "Aquarium.Session.PlaneFitsNarrowAspect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSessionPlaneFitsAspect::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = SpawnGameMode(World, 1, 2);
	// 4:3 at 75 deg horizontal FOV, plane 220 cm ahead of a camera at the origin.
	const FVector2D Fitted = GM->FitPlaneToView(220.f, 75.f, 4.f / 3.f, FVector2D(130.f, 65.f));
	TestTrue(TEXT("half width fits"), Fitted.X <= 130.f);
	TestTrue(TEXT("half height fits"), Fitted.Y <= 65.f);
	TestTrue(TEXT("still usable"), Fitted.X > 40.f && Fitted.Y > 20.f);
	const FVector2D Wide = GM->FitPlaneToView(220.f, 75.f, 21.f / 9.f, FVector2D(130.f, 65.f));
	TestTrue(TEXT("wide screen is not shrunk below the request"), Wide.X == 130.f);
	return true;
}
```

- [ ] **Step 2: 실패 확인** — UE 빌드. Expected: `no member named 'FitPlaneToView'`.

- [ ] **Step 3: 구현**
  - `UPROPERTY(EditAnywhere, Category="Session") float PlayerMaxSpeed = 90.f; float PlayerAccel = 140.f; float PlayerDecel = 180.f;`
  - `SpawnPlayerFish`에서 `bPlayerControlled = true`와 세 속도를 적용(스케일 정규화·이름표 부착은 그대로).
  - `static FVector2D FitPlaneToView(float DistanceCm, float HorizontalFovDeg, float AspectRatio, FVector2D RequestedHalfExtents);` — 보이는 반폭 `D * tan(FOV/2)`, 반높이 `그 값 / Aspect`; 요청값과 비교해 더 작은 쪽을 쓰되 최소 40×20 cm를 보장한다.
  - `BeginSession`에서 뷰포트 크기(`GEngine->GameViewport->Viewport->GetSizeXY()`, 없으면 16:9)와 `DiverCamera`의 FOV로 `FitPlaneToView`를 호출해 스폰 전에 `PlaneHalfWidth/Height`를 갱신한다.

- [ ] **Step 4: 통과 확인** — UE 빌드 두 번 → 테스트. Expected: 34개 Success.

- [ ] **Step 5: 커밋**

```bash
git add unreal/Aquarium/Source
git commit -m "feat: controllable player fish speed and aspect-fitted swim plane (F-07)"
```

---

### Task 5: 시각 검증 영상

**Files:** Create `scripts/render_m3_video.sh`; 산출물 `docs/reviews/<날짜>-m3-{control.mp4,steer.png,wall.png}`

- [ ] **Step 1: 스크립트** — `scripts/render_m2b_video.sh`를 복사해 `-seconds=20`, `-AquariumAutoNickname=니모 -AquariumAssignmentSeed=1 -AquariumAutoInput="R3,U2,L3,D2,0 1,R2,U2"`(테스트 데이터 전용 주석 유지), 출력 `<날짜>-m3-control.mp4`, 스틸 2장(`-ss 5` → `-m3-steer.png`, `-ss 12` → `-m3-wall.png`).

- [ ] **Step 2: 실행과 확인** — 스틸 2장과 12프레임 컨택트 시트를 Read 도구로 본다. 기대: 물고기가 지시대로 오른쪽·위·왼쪽·아래로 움직이고, 벽 근처에서 멈추지 않고 벽을 따라 미끄러지며, 입력이 없는 구간에서 부드럽게 느려진다. 이름표가 계속 따라다닌다. 어긋나면 입력 패턴이나 속도 값을 조정해 최대 3회 재시도.

- [ ] **Step 3: 커밋**

```bash
git add scripts/render_m3_video.sh docs/reviews
git commit -m "feat: M3 control capture script and review video/stills"
```

---

### Task 6: 문서와 마무리

- [ ] **Step 1: 전체 재검증** — ctest(65), UE 빌드, UE 테스트(34), 결과 문자열 확보.
- [ ] **Step 2: 문서** — `docs/TASK.md` M3 행(테스트 수·영상 경로·사용자 검토 대기, 벽 조향 후속 항목 해소 표시), `docs/SETUP.md` 재현 절에 `render_m3_video.sh` 추가, `README.md` 현재 상태.
- [ ] **Step 3: 커밋·푸시**

```bash
git add docs README.md
git commit -m "docs: record M3 results and reproduction commands"
git push -u origin feat/m3-arrow-control
```

- [ ] **Step 4: 사용자에게 영상·스틸 전달 후 superpowers:finishing-a-development-branch**

---

## 범위 밖 (다음 계획)

- M4: 실사 반복(물고기·산호 정밀화, 무리 행동, 소품 충돌, 수직 전환 롤 제거).
- M5: 클릭 도망(F-09~13). M6: 성능 판정·패키징.
