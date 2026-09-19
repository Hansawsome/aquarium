#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "DiverSpectatorPawn.generated.h"

// Spectator pawn with default movement bindings disabled: arrow keys are reserved
// for the fish (P-04), and mouse look must not drift the pawn's ControlRotation.
UCLASS()
class AQUARIUM_API ADiverSpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()
public:
	ADiverSpectatorPawn();
};
