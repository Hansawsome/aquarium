#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AquariumGameMode.h"
#include "DiverPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAquariumGameModeUsesDiverControllerTest, "Aquarium.GameMode.UsesDiverController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAquariumGameModeUsesDiverControllerTest::RunTest(const FString& Parameters)
{
    const AAquariumGameMode* Cdo = GetDefault<AAquariumGameMode>();
    TestTrue(TEXT("player controller class is diver controller"), Cdo->PlayerControllerClass == ADiverPlayerController::StaticClass());
    TestTrue(TEXT("default pawn class is spectator pawn"), Cdo->DefaultPawnClass == ASpectatorPawn::StaticClass());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
