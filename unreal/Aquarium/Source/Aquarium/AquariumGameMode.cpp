#include "AquariumGameMode.h"
#include "DiverPlayerController.h"
#include "GameFramework/SpectatorPawn.h"

AAquariumGameMode::AAquariumGameMode()
{
	PlayerControllerClass = ADiverPlayerController::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();   // no visible pawn; view comes from DiverCamera
}
