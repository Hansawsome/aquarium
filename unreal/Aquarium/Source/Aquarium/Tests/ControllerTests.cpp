#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "DiverPlayerController.h"
#include "FishActor.h"
#include "FishSchoolSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "aquarium/Flee.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerMapsResult, "Aquarium.Controller.MapsSessionResultToEntryError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FControllerMapsResult::RunTest(const FString&)
{
	TestEqual(TEXT("ok"), ADiverPlayerController::EntryErrorFor(EBeginSessionResult::Ok, TEXT("니모")), EEntryError::None);
	TestEqual(TEXT("already active"), ADiverPlayerController::EntryErrorFor(EBeginSessionResult::AlreadyActive, TEXT("니모")), EEntryError::None);
	TestEqual(TEXT("empty catalog"), ADiverPlayerController::EntryErrorFor(EBeginSessionResult::EmptyCatalog, TEXT("니모")), EEntryError::NoFishAvailable);
	TestEqual(TEXT("invalid: blank"), ADiverPlayerController::EntryErrorFor(EBeginSessionResult::InvalidNickname, TEXT("   ")), EEntryError::Empty);
	TestEqual(TEXT("invalid: newline"), ADiverPlayerController::EntryErrorFor(EBeginSessionResult::InvalidNickname, TEXT("니\n모")), EEntryError::InvalidCharacter);
	TestEqual(TEXT("invalid: 13 hangul"), ADiverPlayerController::EntryErrorFor(EBeginSessionResult::InvalidNickname, TEXT("가나다라마바사아자차카타파")), EEntryError::TooLong);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerAutoReplay, "Aquarium.Controller.AutoReplayParsesCommandLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FControllerAutoReplay::RunTest(const FString&)
{
	FString Name;
	float ExitAfter = -1.f;
	TestTrue(TEXT("parses"), ADiverPlayerController::ParseAutoReplay(TEXT("-AquariumAutoNickname=니모 -AquariumAutoExitAfter=8"), Name, ExitAfter));
	TestEqual(TEXT("name"), Name, FString(TEXT("니모")));
	TestEqual(TEXT("exit after"), ExitAfter, 8.f);

	TestTrue(TEXT("nickname only"), ADiverPlayerController::ParseAutoReplay(TEXT("-AquariumAutoNickname=니모"), Name, ExitAfter));
	TestEqual(TEXT("exit defaults to 0"), ExitAfter, 0.f);

	TestFalse(TEXT("empty command line"), ADiverPlayerController::ParseAutoReplay(TEXT(""), Name, ExitAfter));
	TestTrue(TEXT("name cleared"), Name.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerUiCaptureDir, "Aquarium.Controller.ParsesUiCaptureDir",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FControllerUiCaptureDir::RunTest(const FString&)
{
	FString Dir;
	TestTrue(TEXT("parses"), ADiverPlayerController::ParseUiCaptureDir(TEXT("-benchmark -AquariumCaptureUI=/tmp/ui frames -seconds=14"), Dir));
	TestEqual(TEXT("dir"), Dir, FString(TEXT("/tmp/ui")));

	TestTrue(TEXT("quoted path with space"), ADiverPlayerController::ParseUiCaptureDir(TEXT("-AquariumCaptureUI=\"/a b/frames\""), Dir));
	TestEqual(TEXT("quoted dir"), Dir, FString(TEXT("/a b/frames")));

	TestFalse(TEXT("absent"), ADiverPlayerController::ParseUiCaptureDir(TEXT("-AquariumAutoNickname=x"), Dir));
	TestTrue(TEXT("cleared"), Dir.IsEmpty());
	TestFalse(TEXT("empty value"), ADiverPlayerController::ParseUiCaptureDir(TEXT("-AquariumCaptureUI="), Dir));
	TestFalse(TEXT("null"), ADiverPlayerController::ParseUiCaptureDir(nullptr, Dir));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerAssignmentSeed, "Aquarium.Controller.ParsesAssignmentSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FControllerAssignmentSeed::RunTest(const FString&)
{
	int32 Seed = -1;
	TestTrue(TEXT("parses"), ADiverPlayerController::ParseAssignmentSeed(TEXT("-game -AquariumAssignmentSeed=7 -seconds=14"), Seed));
	TestEqual(TEXT("seed"), Seed, 7);
	TestTrue(TEXT("negative"), ADiverPlayerController::ParseAssignmentSeed(TEXT("-AquariumAssignmentSeed=-3"), Seed));
	TestEqual(TEXT("negative seed"), Seed, -3);
	TestFalse(TEXT("zero is rejected"), ADiverPlayerController::ParseAssignmentSeed(TEXT("-AquariumAssignmentSeed=0"), Seed));
	TestFalse(TEXT("non-numeric"), ADiverPlayerController::ParseAssignmentSeed(TEXT("-AquariumAssignmentSeed=abc"), Seed));
	TestFalse(TEXT("absent"), ADiverPlayerController::ParseAssignmentSeed(TEXT("-AquariumAutoNickname=x"), Seed));
	TestEqual(TEXT("cleared"), Seed, 0);
	TestFalse(TEXT("null"), ADiverPlayerController::ParseAssignmentSeed(nullptr, Seed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerFrameStatsPath, "Aquarium.Controller.ParsesFrameStatsPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FControllerFrameStatsPath::RunTest(const FString&)
{
	FString Path;
	TestTrue(TEXT("parses"), ADiverPlayerController::ParseFrameStatsPath(TEXT("-game -AquariumFrameStats=/tmp/x.csv -unattended"), Path));
	TestEqual(TEXT("path"), Path, FString(TEXT("/tmp/x.csv")));

	TestTrue(TEXT("quoted path with space"), ADiverPlayerController::ParseFrameStatsPath(TEXT("-AquariumFrameStats=\"/a b/frames.csv\""), Path));
	TestEqual(TEXT("quoted path"), Path, FString(TEXT("/a b/frames.csv")));

	TestFalse(TEXT("absent"), ADiverPlayerController::ParseFrameStatsPath(TEXT("-AquariumAutoNickname=x"), Path));
	TestTrue(TEXT("cleared"), Path.IsEmpty());
	TestFalse(TEXT("empty value"), ADiverPlayerController::ParseFrameStatsPath(TEXT("-AquariumFrameStats="), Path));
	TestFalse(TEXT("null"), ADiverPlayerController::ParseFrameStatsPath(nullptr, Path));
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerAutoInput, "Aquarium.Controller.ParsesAutoInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FControllerAutoInput::RunTest(const FString&)
{
	FString Pattern;
	TestTrue(TEXT("parses"), ADiverPlayerController::ParseAutoInput(TEXT("-game -AquariumAutoInput=\"R3,U2\" -unattended"), Pattern));
	TestEqual(TEXT("pattern"), Pattern, FString(TEXT("R3,U2")));

	TestTrue(TEXT("unquoted"), ADiverPlayerController::ParseAutoInput(TEXT("-AquariumAutoInput=R3,U2"), Pattern));
	TestEqual(TEXT("unquoted pattern"), Pattern, FString(TEXT("R3,U2")));

	TestFalse(TEXT("absent"), ADiverPlayerController::ParseAutoInput(TEXT("-AquariumAutoNickname=x"), Pattern));
	TestTrue(TEXT("cleared"), Pattern.IsEmpty());
	TestFalse(TEXT("empty value"), ADiverPlayerController::ParseAutoInput(TEXT("-AquariumAutoInput="), Pattern));
	TestFalse(TEXT("null"), ADiverPlayerController::ParseAutoInput(nullptr, Pattern));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerClearsHeldKeysOnSessionChange, "Aquarium.Controller.ClearsHeldKeysOnSessionChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FControllerClearsHeldKeysOnSessionChange::RunTest(const FString&)
{
	// Regression for: a child holds Right and exits (Esc / HUD exit button) while ShowEntry()
	// installs FInputModeUIOnly, which swallows the IE_Released -- ArrowKeys.bRight stays true and
	// drives the NEXT session's fish from frame one with no key actually held.
	// ShowEntry()/ShowSession() both bail out early when Entry/Hud are unset (as they are for a
	// bare NewObject controller, headless), but ResetArrowKeys() now runs before that guard, so
	// this exercises the real fix without needing a live widget tree.
	ADiverPlayerController* Controller = NewObject<ADiverPlayerController>();
	if (!TestNotNull(TEXT("controller"), Controller))
	{
		return false;
	}

	Controller->ArrowKeys.bRight = true;
	Controller->ShowEntry();
	TestFalse(TEXT("ShowEntry clears bRight"), Controller->ArrowKeys.bRight);

	Controller->ArrowKeys.bUp = true;
	Controller->ArrowKeys.bLeft = true;
	Controller->ShowSession();
	TestFalse(TEXT("ShowSession clears bUp"), Controller->ArrowKeys.bUp);
	TestFalse(TEXT("ShowSession clears bLeft"), Controller->ArrowKeys.bLeft);
	return true;
}

// F-11: mashing. Same fish mid-flee is ignored; same fish during recovery restarts the flee;
// a different fish is independent.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerRepeatedClicks, "Aquarium.Controller.RepeatedClicksFollowFleeRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControllerRepeatedClicks::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* A = World->SpawnActor<AFishActor>();
	AFishActor* B = World->SpawnActor<AFishActor>();
	A->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	B->PlaneOrigin = FVector(400.f, 200.f, 100.f);
	A->InitializeSwim();
	B->InitializeSwim();

	// First click on A, from its screen-left: flee goes right.
	A->ApplyFleeFrom(A->GetActorLocation() - FVector(0.f, 20.f, 0.f));
	const FVector FirstDirProbe = A->GetActorLocation();
	for (int32 i = 0; i < 12; ++i) { A->StepSwim(1.f / 60.f); }   // 0.2 s in
	const bool bMovingRight = A->GetActorLocation().Y > FirstDirProbe.Y;
	TestTrue(TEXT("first flee goes right"), bMovingRight);

	// Re-click A from the OTHER side while still fleeing. **M7 사양 변경**: M5의 F-11은
	// 이것을 무시했지만 시나리오 장면 2 요구사항 4가 연타를 기본 사용법으로 못 박았다.
	// 이제 무시하지 않고 다시 겨눈다 -- 속도가 실제로 왼쪽으로 돌아서야 한다.
	A->ApplyFleeFrom(A->GetActorLocation() + FVector(0.f, 20.f, 0.f));
	const double YReaim = A->GetActorLocation().Y;
	A->StepSwim(1.f / 60.f);
	const double VJustAfter = A->GetActorLocation().Y - YReaim;
	for (int32 i = 0; i < 30; ++i) { A->StepSwim(1.f / 60.f); }
	const double YLate = A->GetActorLocation().Y;
	A->StepSwim(1.f / 60.f);
	const double VLate = A->GetActorLocation().Y - YLate;
	TestTrue(FString::Printf(TEXT("mid-flee re-click re-aims (%.4f -> %.4f cm/frame)"), VJustAfter, VLate),
		VLate < VJustAfter);

	// Click B while A is fleeing: independent.
	TestTrue(TEXT("B untouched so far"), B->FleeState() == aquarium::BehaviorState::Normal);
	B->ApplyFleeFrom(B->GetActorLocation() - FVector(0.f, 20.f, 0.f));
	TestTrue(TEXT("B flees"), B->FleeState() == aquarium::BehaviorState::Fleeing);
	TestTrue(TEXT("A still fleeing on its own timer"), A->FleeState() == aquarium::BehaviorState::Fleeing);

	// Run A into recovery, then re-click from the other side: must RESTART the flee.
	for (int32 i = 0; i < 36; ++i) { A->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("A recovering"), A->FleeState() == aquarium::BehaviorState::Recovering);
	A->ApplyFleeFrom(A->GetActorLocation() + FVector(0.f, 20.f, 0.f));
	TestTrue(TEXT("recovery re-click restarts the flee"), A->FleeState() == aquarium::BehaviorState::Fleeing);
	// PLAN DEVIATION, see the report: the plan compared POSITIONS 0.4 s after the restart, which a
	// reversal cannot achieve -- aquarium::StepMotion turns the velocity at accel = 30 cm/s^2, so
	// a ~30 cm/s rightward fish needs ~1 s to be left of where it started and the flee lasts 0.8 s.
	// The velocity is what actually reverses, so it is what is measured.
	const double YRestart = A->GetActorLocation().Y;
	A->StepSwim(1.f / 60.f);
	const double VBefore = A->GetActorLocation().Y - YRestart;
	for (int32 i = 0; i < 45; ++i) { A->StepSwim(1.f / 60.f); }
	const double YNearEnd = A->GetActorLocation().Y;
	A->StepSwim(1.f / 60.f);
	const double VAfter = A->GetActorLocation().Y - YNearEnd;
	TestTrue(FString::Printf(TEXT("and it now goes the other way (%.4f -> %.4f cm/frame)"), VBefore, VAfter),
		VAfter < VBefore);
	return true;
}

// F-09: a click with no active session (the entry screen is up) disturbs nothing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerClickNeedsSession, "Aquarium.Controller.ClickIgnoredWithoutSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControllerClickNeedsSession::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Mesh)) return false;
	AFishActor* F = World->SpawnActor<AFishActor>();
	F->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	F->SetMesh(Mesh);
	F->InitializeSwim();
	School->Register(F);
	// The same ray WOULD hit this fish, so the guard is what makes the click a no-op below.
	FVector Probe = FVector::ZeroVector;
	TestTrue(TEXT("the ray does hit the fish"),
		School->PickFrontmostHit(FVector(0.f, 0.f, 100.f), FVector(1.f, 0.f, 0.f), Probe) == F);

	ADiverPlayerController* PC = World->SpawnActor<ADiverPlayerController>();
	// No game mode session was ever begun, so the entry screen would be up.
	const bool bHandled = PC->HandleClickRay(FVector(0.f, 0.f, 100.f), FVector(1.f, 0.f, 0.f));
	TestFalse(TEXT("click is not handled without a session"), bHandled);
	TestTrue(TEXT("no fish was disturbed"), F->FleeState() == aquarium::BehaviorState::Normal);
	return true;
}


// Dev-only scripted clicks. The grammar is pinned here because the -AquariumAutoInput parser
// taught this project that a silently-ignored bad token produces a convincing but empty capture.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerAutoClickParses, "Aquarium.Controller.AutoClickPatternParses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControllerAutoClickParses::RunTest(const FString&)
{
	FString Pattern;
	TestTrue(TEXT("flag is found"), ADiverPlayerController::ParseAutoClick(
		TEXT("-AquariumAutoClick=4.0@0.50x0.46,7.25@0.12x0.80"), Pattern));
	TestEqual(TEXT("pattern survives the commas"), Pattern, FString(TEXT("4.0@0.50x0.46,7.25@0.12x0.80")));
	TestFalse(TEXT("absent flag"), ADiverPlayerController::ParseAutoClick(TEXT("-Other=1"), Pattern));
	TestFalse(TEXT("null command line"), ADiverPlayerController::ParseAutoClick(nullptr, Pattern));

	const TArray<ADiverPlayerController::FAutoClick> Good =
		ADiverPlayerController::BuildAutoClicks(TEXT("7.25@0.12x0.80, 4.0@0.50x0.46"));
	TestEqual(TEXT("two clicks"), Good.Num(), 2);
	// Written out of order on purpose: they must fire in time order.
	TestEqual(TEXT("first time"), Good[0].TimeSeconds, 4.0f, 1e-3f);
	TestEqual(TEXT("first nx"), static_cast<float>(Good[0].Normalized.X), 0.50f, 1e-3f);
	TestEqual(TEXT("first ny"), static_cast<float>(Good[0].Normalized.Y), 0.46f, 1e-3f);
	TestEqual(TEXT("second time"), Good[1].TimeSeconds, 7.25f, 1e-3f);

	// Every one of these is the kind of token an author invents from memory. All must be dropped.
	const TArray<ADiverPlayerController::FAutoClick> Bad = ADiverPlayerController::BuildAutoClicks(
		TEXT("4.0:0.5x0.5,4.0@0.5,4.0@0.5x0.5x0.5,@0.5x0.5,4.0@1.5x0.5,4.0@-0.1x0.5,-1@0.5x0.5,abc@0.5x0.5"));
	TestEqual(TEXT("every malformed token is dropped"), Bad.Num(), 0);

	FString ClickCsvPath;
	TestTrue(TEXT("click log path is found"), ADiverPlayerController::ParseClickLogPath(
		TEXT("-AquariumClickLog=/tmp/clicks.csv"), ClickCsvPath));
	TestEqual(TEXT("click log path survives"), ClickCsvPath, FString(TEXT("/tmp/clicks.csv")));
	TestFalse(TEXT("absent click log flag"), ADiverPlayerController::ParseClickLogPath(TEXT("-Other=1"), ClickCsvPath));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
