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
	// The authored request. PlaneHalfWidth/PlaneHalfHeight below are the *fitted* values and are
	// recomputed from this every session, so a fit can never compound on a previous one.
	UPROPERTY(EditAnywhere, Category = "Session") FVector2D RequestedPlaneHalfExtents = FVector2D(130.f, 65.f);
	UPROPERTY(VisibleAnywhere, Category = "Session") float PlaneHalfWidth = 130.f;
	UPROPERTY(VisibleAnywhere, Category = "Session") float PlaneHalfHeight = 65.f;
	// FitSwimPlaneToViewport가 실제로 쓴 값. 기포의 소멸 높이가 이것에서 파생되므로
	// 시야각·화면 비율을 두 번 읽지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "Session") float ViewFovDeg = 75.f;
	UPROPERTY(VisibleAnywhere, Category = "Session") float ViewAspect = 16.f / 9.f;
	// The player's fish is normalized to this body length (cm) whatever species the session
	// assigns: the catalog spans 9 cm (Damselfish) to 32 cm (BlueTang), so at native scale a
	// small assignment reads SMALLER than the background fish. Combined with the nearer plane
	// at X=220 (background fish sit at X >= 330) this makes it the biggest fish on screen.
	// Background fish keep their own 0.75-1.3 scatter scale from Scripts/build_reef_m1.py.
	UPROPERTY(EditAnywhere, Category = "Session") float PlayerFishTargetLengthCm = 34.f;
	// The player's fish swims faster and responds harder than a background fish (MaxSpeed 40) so
	// arrow-key input feels immediate rather than like nudging a drifting object (F-07).
	UPROPERTY(EditAnywhere, Category = "Session") float PlayerMaxSpeed = 90.f;   // cm/s
	UPROPERTY(EditAnywhere, Category = "Session") float PlayerAccel = 140.f;
	UPROPERTY(EditAnywhere, Category = "Session") float PlayerDecel = 180.f;

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

	// Shrinks the requested swim-plane half extents so the whole plane stays inside the camera
	// frustum at DistanceCm. Pure, so it is testable headless. The visible half width is
	// DistanceCm * tan(Fov/2) with a small inset, the half height that divided by AspectRatio;
	// the result is the component-wise minimum against the request, floored so it stays usable.
	static FVector2D FitPlaneToView(float DistanceCm, float HorizontalFovDeg, float AspectRatio, FVector2D RequestedHalfExtents);
	// 주어진 깊이에서 **실제로 보이는** 반 크기(여백 없음). FitPlaneToView가 쓰는
	// 것과 같은 식이며, 같은 공식을 두 번 적지 않으려고 여기로 뺐다(규약 7).
	static FVector2D VisibleHalfExtents(float DistanceCm, float HorizontalFovDeg, float AspectRatio);
	// 그 깊이에서 화면 위 끝의 월드 Z. 기포가 사라지는 높이가 여기서 파생된다.
	float ScreenTopZAt(float DepthCm) const;

	// Sets PlaneHalfWidth/PlaneHalfHeight from RequestedPlaneHalfExtents, the live viewport aspect
	// and the DiverCamera FOV. Idempotent: it always fits the authored request, never the current
	// (already fitted) extents, so repeated sessions cannot shrink the plane step by step.
	// Public so the automation tests can drive it without a session.
	void FitSwimPlaneToViewport();

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
