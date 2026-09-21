#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AquariumGameMode.h"
#include "FishActor.h"

namespace
{
AAquariumGameMode* SpawnGameMode(UWorld* World, int32 Seed, int32 SpeciesCount)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAquariumGameMode* GM = World->SpawnActor<AAquariumGameMode>(AAquariumGameMode::StaticClass(), FTransform::Identity, P);
	TArray<FFishSpecies> Catalog;
	if (SpeciesCount >= 1)
	{
		FFishSpecies S;
		S.DisplayName = FText::FromString(TEXT("블루탱"));
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang")));
		Catalog.Add(S);
	}
	if (SpeciesCount >= 2)
	{
		FFishSpecies S;
		S.DisplayName = FText::FromString(TEXT("흰동가리"));
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/Clownfish/SK_Clownfish.SK_Clownfish")));
		Catalog.Add(S);
	}
	if (SpeciesCount >= 3)
	{
		FFishSpecies S;
		S.DisplayName = FText::FromString(TEXT("노란탱"));
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/YellowTang/SK_YellowTang.SK_YellowTang")));
		Catalog.Add(S);
	}
	if (SpeciesCount >= 4)
	{
		FFishSpecies S;
		S.DisplayName = FText::FromString(TEXT("나비고기"));
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/Butterflyfish/SK_Butterflyfish.SK_Butterflyfish")));
		Catalog.Add(S);
	}
	if (SpeciesCount >= 5)
	{
		FFishSpecies S;
		S.DisplayName = FText::FromString(TEXT("담셀피시"));
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/Damselfish/SK_Damselfish.SK_Damselfish")));
		Catalog.Add(S);
	}
	GM->SetCatalogForTest(Catalog, Seed);
	return GM;
}

// Destroyed actors may still be returned by the iterator until GC; skip them.
int32 CountPlayerFish(UWorld* World)
{
	int32 N = 0;
	for (TActorIterator<AFishActor> It(World); It; ++It)
	{
		if (It->bIsPlayerFish && IsValid(*It) && !It->IsActorBeingDestroyed())
		{
			++N;
		}
	}
	return N;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionBeginSpawnsOnePlayerFish, "Aquarium.Session.BeginSpawnsOnePlayerFish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSessionBeginSpawnsOnePlayerFish::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = SpawnGameMode(World, 1, 2);
	TestEqual(TEXT("Ok"), GM->BeginSession(TEXT("  니모 ")), EBeginSessionResult::Ok);
	TestTrue(TEXT("active"), GM->HasActiveSession());
	TestEqual(TEXT("nickname trimmed"), GM->CurrentNickname(), FString(TEXT("니모")));
	TestEqual(TEXT("one player fish"), CountPlayerFish(World), 1);
	TestNotNull(TEXT("player fish"), GM->PlayerFish());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionDoubleBeginRejected, "Aquarium.Session.DoubleBeginIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSessionDoubleBeginRejected::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = SpawnGameMode(World, 1, 2);
	GM->BeginSession(TEXT("니모"));
	TestEqual(TEXT("AlreadyActive"), GM->BeginSession(TEXT("도리")), EBeginSessionResult::AlreadyActive);
	TestEqual(TEXT("still one fish"), CountPlayerFish(World), 1);
	TestEqual(TEXT("first nickname kept"), GM->CurrentNickname(), FString(TEXT("니모")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionInvalidInputsSpawnNothing, "Aquarium.Session.InvalidInputsSpawnNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSessionInvalidInputsSpawnNothing::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = SpawnGameMode(World, 1, 2);
	TestEqual(TEXT("empty"), GM->BeginSession(TEXT("   ")), EBeginSessionResult::InvalidNickname);
	TestEqual(TEXT("too long"), GM->BeginSession(TEXT("가나다라마바사아자차카타파")), EBeginSessionResult::InvalidNickname);
	TestEqual(TEXT("no fish"), CountPlayerFish(World), 0);
	AAquariumGameMode* Empty = SpawnGameMode(World, 1, 0);
	TestEqual(TEXT("empty catalog"), Empty->BeginSession(TEXT("니모")), EBeginSessionResult::EmptyCatalog);
	TestFalse(TEXT("not active"), Empty->HasActiveSession());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionSeedDeterminesSpecies, "Aquarium.Session.SameSeedSameSpecies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSessionSeedDeterminesSpecies::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* A = SpawnGameMode(World, 42, 5);
	AAquariumGameMode* B = SpawnGameMode(World, 42, 5);
	A->BeginSession(TEXT("니모"));
	B->BeginSession(TEXT("니모"));
	TestEqual(TEXT("same species index"), A->AssignedSpeciesIndex(), B->AssignedSpeciesIndex());
	TestEqual(TEXT("five species loaded"), A->LoadedSpeciesCount(), 5);
	TSet<int32> Seen;
	for (int32 Seed = 1; Seed <= 60; ++Seed)
	{
		AAquariumGameMode* G = SpawnGameMode(World, Seed, 5);
		G->BeginSession(TEXT("x"));
		Seen.Add(G->AssignedSpeciesIndex());
	}
	for (int32 Index = 0; Index < 5; ++Index)
	{
		TestTrue(FString::Printf(TEXT("species index %d reachable"), Index), Seen.Contains(Index));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionEndClearsAndReentryIsFresh, "Aquarium.Session.EndClearsAndReentryIsFresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSessionEndClearsAndReentryIsFresh::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = SpawnGameMode(World, 3, 2);
	GM->BeginSession(TEXT("니모"));
	GM->EndSession();
	TestFalse(TEXT("inactive"), GM->HasActiveSession());
	TestTrue(TEXT("nickname cleared"), GM->CurrentNickname().IsEmpty());
	TestEqual(TEXT("no player fish"), CountPlayerFish(World), 0);
	TestEqual(TEXT("re-entry ok"), GM->BeginSession(TEXT("도리")), EBeginSessionResult::Ok);
	TestEqual(TEXT("new nickname"), GM->CurrentNickname(), FString(TEXT("도리")));
	TestEqual(TEXT("one player fish again"), CountPlayerFish(World), 1);
	return true;
}

// The player's fish must read as the biggest fish on screen regardless of which species the
// session assigns, so the game mode normalizes it to PlayerFishTargetLengthCm.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionPlayerFishIsNormalizedSize, "Aquarium.Session.PlayerFishIsNormalizedSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSessionPlayerFishIsNormalizedSize::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();

	// One-species catalogs so the assignment is forced.
	auto SpawnSingle = [World](const TCHAR* MeshPath) -> AAquariumGameMode*
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAquariumGameMode* GM = World->SpawnActor<AAquariumGameMode>(AAquariumGameMode::StaticClass(), FTransform::Identity, P);
		TArray<FFishSpecies> Catalog;
		FFishSpecies S;
		S.DisplayName = FText::FromString(TEXT("종"));
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath));
		Catalog.Add(S);
		GM->SetCatalogForTest(Catalog, 7);
		return GM;
	};

	AAquariumGameMode* Small = SpawnSingle(TEXT("/Game/Fish/Damselfish/SK_Damselfish.SK_Damselfish"));
	TestEqual(TEXT("small ok"), Small->BeginSession(TEXT("니모")), EBeginSessionResult::Ok);
	AFishActor* SmallFish = Small->PlayerFish();
	TestNotNull(TEXT("small fish"), SmallFish);

	AAquariumGameMode* Large = SpawnSingle(TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	TestEqual(TEXT("large ok"), Large->BeginSession(TEXT("도리")), EBeginSessionResult::Ok);
	AFishActor* LargeFish = Large->PlayerFish();
	TestNotNull(TEXT("large fish"), LargeFish);

	if (SmallFish == nullptr || LargeFish == nullptr)
	{
		return false;
	}

	const float Target = Small->PlayerFishTargetLengthCm;
	const float SmallLength = SmallFish->GetComponentsBoundingBox(true).GetExtent().X * 2.f;
	const float LargeLength = LargeFish->GetComponentsBoundingBox(true).GetExtent().X * 2.f;
	TestTrue(FString::Printf(TEXT("small species normalized to %.1f cm (got %.1f)"), Target, SmallLength),
		FMath::Abs(SmallLength - Target) <= Target * 0.15f);
	TestTrue(FString::Printf(TEXT("large species normalized to %.1f cm (got %.1f)"), Target, LargeLength),
		FMath::Abs(LargeLength - Target) <= Target * 0.15f);

	// The small species has to be scaled up far more than the large one.
	const float SmallScale = SmallFish->GetActorScale3D().X;
	const float LargeScale = LargeFish->GetActorScale3D().X;
	TestTrue(FString::Printf(TEXT("scales differ by >1.5x (small %.2f, large %.2f)"), SmallScale, LargeScale),
		SmallScale > LargeScale * 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionPlayerFishIsControllable, "Aquarium.Session.PlayerFishIsControllable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionPlayerFishIsControllable::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = SpawnGameMode(World, 1, 2);
	GM->BeginSession(TEXT("니모"));
	AFishActor* Fish = GM->PlayerFish();
	if (!TestNotNull(TEXT("player fish"), Fish)) return false;
	TestTrue(TEXT("player controlled"), Fish->bPlayerControlled);
	TestTrue(TEXT("faster than background"), Fish->MaxSpeed >= 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionPlaneFitsAspect, "Aquarium.Session.PlaneFitsNarrowAspect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionPlaneFitsAspect::RunTest(const FString&)
{
	// At 220 cm / 75 deg / 4:3 the visible extents (~155 x 67 cm) are LARGER than the requested
	// 130 x 65, so the old params never made the fit bind: every assertion below passed even for a
	// stub that returns the request unchanged. Distance=220 / Fov=40 / Aspect=1.2 instead yields
	// visible half extents smaller than the request on both axes, so this now genuinely exercises
	// the clamp. Expected values are computed here from the same formula FitPlaneToView documents
	// (VisibleHalfWidth = Distance * tan(Fov/2) * 0.92, VisibleHalfHeight = VisibleHalfWidth /
	// Aspect, floored at 40/20) rather than just asserting <=, so a stub cannot pass by accident.
	constexpr float kInset = 0.92f;
	constexpr float kTolerance = 0.05f;
	const float Distance = 220.f;
	const float Fov = 40.f;
	const float Aspect = 1.2f;
	const FVector2D Request(130.f, 65.f);
	const float ExpectedVisibleHalfWidth = Distance * FMath::Tan(FMath::DegreesToRadians(Fov) * 0.5f) * kInset;
	const float ExpectedVisibleHalfHeight = ExpectedVisibleHalfWidth / Aspect;
	// Both axes are expected to bind (be smaller than the request) for this pick of parameters.
	TestTrue(TEXT("width actually binds for this test"), ExpectedVisibleHalfWidth < Request.X);
	TestTrue(TEXT("height actually binds for this test"), ExpectedVisibleHalfHeight < Request.Y);

	const FVector2D Fitted = AAquariumGameMode::FitPlaneToView(Distance, Fov, Aspect, Request);
	TestTrue(TEXT("half width matches the documented formula"),
		FMath::IsNearlyEqual(static_cast<float>(Fitted.X), ExpectedVisibleHalfWidth, kTolerance));
	TestTrue(TEXT("half height matches the documented formula"),
		FMath::IsNearlyEqual(static_cast<float>(Fitted.Y), ExpectedVisibleHalfHeight, kTolerance));
	TestTrue(TEXT("still usable"), Fitted.X > 40.f && Fitted.Y > 20.f);

	// A wide viewport (Distance=220 / Fov=75 / 21:9) leaves the request untouched: its visible
	// extents (~155 x 67 cm) are larger than the request on both axes.
	const FVector2D Wide = AAquariumGameMode::FitPlaneToView(220.f, 75.f, 21.f / 9.f, Request);
	TestTrue(TEXT("wide screen keeps the requested width"), FMath::IsNearlyEqual(static_cast<float>(Wide.X), Request.X, 0.01f));
	TestTrue(TEXT("wide screen keeps the requested height"), FMath::IsNearlyEqual(static_cast<float>(Wide.Y), Request.Y, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionPlaneRefitsFromRequest, "Aquarium.Session.PlaneRefitsFromRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionPlaneRefitsFromRequest::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = SpawnGameMode(World, 1, 2);
	GM->RequestedPlaneHalfExtents = FVector2D(130.f, 65.f);

	GM->FitSwimPlaneToViewport();
	const float First = GM->PlaneHalfWidth;
	const float FirstHeight = GM->PlaneHalfHeight;

	// A second session must produce the same plane, not a plane fitted to the first fit's output.
	GM->FitSwimPlaneToViewport();
	TestEqual(TEXT("second fit matches the first"), GM->PlaneHalfWidth, First);
	TestEqual(TEXT("second fit height matches"), GM->PlaneHalfHeight, FirstHeight);

	// Even if the extents were narrowed in between (a smaller window), the fit restores them.
	GM->PlaneHalfWidth = 45.f;
	GM->PlaneHalfHeight = 25.f;
	GM->FitSwimPlaneToViewport();
	TestEqual(TEXT("fit ignores the current extents"), GM->PlaneHalfWidth, First);
	TestEqual(TEXT("height ignores the current extents"), GM->PlaneHalfHeight, FirstHeight);

	TestTrue(TEXT("never wider than the request"), First <= 130.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
