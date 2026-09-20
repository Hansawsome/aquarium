#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

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

#endif // WITH_DEV_AUTOMATION_TESTS
