#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DiverPlayerController.h"

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

#endif
