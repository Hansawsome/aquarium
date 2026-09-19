#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DiverPlayerController.generated.h"

// Looks through the actor tagged "DiverCamera"; never follows the fish (P-06).
UCLASS()
class AQUARIUM_API ADiverPlayerController : public APlayerController
{
	GENERATED_BODY()
protected:
	virtual void BeginPlay() override;
};
