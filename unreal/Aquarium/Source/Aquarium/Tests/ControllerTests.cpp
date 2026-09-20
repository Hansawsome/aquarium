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

#endif
