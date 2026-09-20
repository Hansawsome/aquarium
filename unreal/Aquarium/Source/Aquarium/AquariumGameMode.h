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
	// Swim plane given to the player fish: closer than background fish so the player's fish reads
	// larger (perspective); fits the 75° FOV at 2.2 m. Background fish (Scripts/build_reef_m1.py)
	// stay at X >= 330.
	UPROPERTY(EditAnywhere, Category = "Session") FVector PlaneOrigin = FVector(220.f, 0.f, 105.f);
	UPROPERTY(EditAnywhere, Category = "Session") float PlaneHalfWidth = 130.f;
	UPROPERTY(EditAnywhere, Category = "Session") float PlaneHalfHeight = 65.f;
	// The player's fish is normalized to this body length (cm) whatever species the session
	// assigns: the catalog spans 9 cm (Damselfish) to 32 cm (BlueTang), so at native scale a
	// small assignment reads SMALLER than the background fish. Combined with the nearer plane
	// at X=220 (background fish sit at X >= 330) this makes it the biggest fish on screen.
	// Background fish keep their own 0.75-1.3 scatter scale from Scripts/build_reef_m1.py.
	UPROPERTY(EditAnywhere, Category = "Session") float PlayerFishTargetLengthCm = 34.f;

	EBeginSessionResult BeginSession(const FString& RawNickname);
	void EndSession();
	bool HasActiveSession() const;
	FString CurrentNickname() const;
	// Index into Catalog (not the loaded subset) of the assigned species; INDEX_NONE when inactive.
	int32 AssignedSpeciesIndex() const;
	AFishActor* PlayerFish() const;
	// Number of catalog entries whose mesh loaded (the pool the session picks from).
	int32 LoadedSpeciesCount() const { return LoadedCatalogIndices.Num(); }

	// Replaces the catalog, reseeds and reloads meshes without waiting for BeginPlay.
	// Ends any active session first so loaded indices cannot dangle.
	void SetCatalogForTest(const TArray<FFishSpecies>& InCatalog, int32 Seed);
	// Re-seeds the species/swim RNG for reproducible captures; ignored (false) while a session
	// is active or when Seed is 0.
	bool SetAssignmentSeed(int32 Seed);

protected:
	virtual void BeginPlay() override;

private:
	aquarium::SessionManager Session;
	FRandomStream Rng;
	UPROPERTY() TObjectPtr<AFishActor> PlayerFishActor = nullptr;
	// Catalog indices whose mesh loaded; the session picks among these.
	TArray<int32> LoadedCatalogIndices;
	// Hard references parallel to LoadedCatalogIndices. TSoftObjectPtr::Get() alone does not
	// keep the mesh alive between BeginPlay and BeginSession in a cooked build.
	UPROPERTY() TArray<TObjectPtr<USkeletalMesh>> LoadedMeshes;

	void RebuildLoadedCatalog();
	AFishActor* SpawnPlayerFish(USkeletalMesh* Mesh);
};
