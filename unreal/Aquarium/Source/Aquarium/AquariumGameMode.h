#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "aquarium/Session.h"

#include "FishSpecies.h"
#include "AquariumGameMode.generated.h"

class AFishActor;

// Mirrors aquarium::BeginResult so Blueprint/UI code never touches the rules layer directly.
UENUM()
enum class EBeginSessionResult : uint8 { Ok, InvalidNickname, EmptyCatalog, AlreadyActive };

// Owns the species catalog (F-02), the single player session (F-03) and the player's fish (F-14).
// Session rules (nickname validation, species pick, re-entry) live in aquarium::SessionManager;
// this class only maps them onto engine objects. The nickname is never logged.
UCLASS()
class AQUARIUM_API AAquariumGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAquariumGameMode();

	UPROPERTY(EditAnywhere, Category = "Session") TArray<FFishSpecies> Catalog;
	// 0 -> seeded from the wall clock at BeginPlay; otherwise reproducible.
	UPROPERTY(EditAnywhere, Category = "Session") int32 AssignmentSeed = 0;
	// Swim plane given to the player fish; matches Scripts/build_reef_m1.py.
	UPROPERTY(EditAnywhere, Category = "Session") FVector PlaneOrigin = FVector(330.f, 0.f, 110.f);
	UPROPERTY(EditAnywhere, Category = "Session") float PlaneHalfWidth = 200.f;
	UPROPERTY(EditAnywhere, Category = "Session") float PlaneHalfHeight = 100.f;

	EBeginSessionResult BeginSession(const FString& RawNickname);
	void EndSession();
	bool HasActiveSession() const;
	FString CurrentNickname() const;
	// Index into Catalog (not the loaded subset) of the assigned species; INDEX_NONE when inactive.
	int32 AssignedSpeciesIndex() const;
	AFishActor* PlayerFish() const;

	// Replaces the catalog, reseeds and reloads meshes without waiting for BeginPlay.
	void SetCatalogForTest(const TArray<FFishSpecies>& InCatalog, int32 Seed);

	virtual void BeginPlay() override;

private:
	aquarium::SessionManager Session;
	FRandomStream Rng;
	UPROPERTY() TObjectPtr<AFishActor> PlayerFishActor = nullptr;
	// Catalog indices whose mesh loaded; the session picks among these.
	TArray<int32> LoadedCatalogIndices;

	void RebuildLoadedCatalog();
	AFishActor* SpawnPlayerFish(USkeletalMesh* Mesh);
};
