#include "AquariumGameMode.h"
#include "DiverPlayerController.h"
#include "DiverSpectatorPawn.h"

AAquariumGameMode::AAquariumGameMode()
{
	PlayerControllerClass = ADiverPlayerController::StaticClass();
	DefaultPawnClass = ADiverSpectatorPawn::StaticClass();   // no visible pawn; view comes from DiverCamera
}
