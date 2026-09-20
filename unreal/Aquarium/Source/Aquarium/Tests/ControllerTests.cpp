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

#endif
