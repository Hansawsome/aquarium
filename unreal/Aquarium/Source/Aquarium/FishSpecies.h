#pragma once

#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"

#include "FishSpecies.generated.h"

// One entry of the fish catalog (SRS F-02): a display name and the skeletal mesh used
// when a fish of this species is spawned for a player.
USTRUCT(BlueprintType)
struct AQUARIUM_API FFishSpecies
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) FText DisplayName;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<USkeletalMesh> Mesh;
};
