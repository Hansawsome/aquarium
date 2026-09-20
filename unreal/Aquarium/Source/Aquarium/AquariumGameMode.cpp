#include "AquariumGameMode.h"
#include "DiverPlayerController.h"
#include "DiverSpectatorPawn.h"
#include "FishActor.h"
#include "Engine/World.h"

#include <string>

AAquariumGameMode::AAquariumGameMode()
{
	PlayerControllerClass = ADiverPlayerController::StaticClass();
	DefaultPawnClass = ADiverSpectatorPawn::StaticClass();   // no visible pawn; view comes from DiverCamera

	FFishSpecies BlueTang;
	BlueTang.DisplayName = NSLOCTEXT("Aquarium", "SpeciesBlueTang", "블루탱");
	BlueTang.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang")));
	Catalog.Add(BlueTang);

	FFishSpecies Clownfish;
	Clownfish.DisplayName = NSLOCTEXT("Aquarium", "SpeciesClownfish", "흰동가리");
	Clownfish.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/Clownfish/SK_Clownfish.SK_Clownfish")));
	Catalog.Add(Clownfish);
}

void AAquariumGameMode::BeginPlay()
{
	Super::BeginPlay();
	const int32 Seed = AssignmentSeed != 0 ? AssignmentSeed : static_cast<int32>(FDateTime::Now().GetTicks());
	Rng.Initialize(Seed);
	RebuildLoadedCatalog();
}

void AAquariumGameMode::SetCatalogForTest(const TArray<FFishSpecies>& InCatalog, int32 Seed)
{
	Catalog = InCatalog;
	AssignmentSeed = Seed;
	Rng.Initialize(Seed);
	RebuildLoadedCatalog();
}

void AAquariumGameMode::RebuildLoadedCatalog()
{
	LoadedCatalogIndices.Reset();
	for (int32 i = 0; i < Catalog.Num(); ++i)
	{
		if (Catalog[i].Mesh.LoadSynchronous() != nullptr)
		{
			LoadedCatalogIndices.Add(i);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AquariumGameMode: catalog entry %d mesh '%s' failed to load; species excluded"),
				i, *Catalog[i].Mesh.ToString());
		}
	}
}

EBeginSessionResult AAquariumGameMode::BeginSession(const FString& RawNickname)
{
	const std::string Raw(TCHAR_TO_UTF8(*RawNickname));
	const aquarium::BeginResult R = Session.Begin(Raw, static_cast<size_t>(LoadedCatalogIndices.Num()),
		[this](size_t Bound) { return static_cast<size_t>(Rng.RandRange(0, static_cast<int32>(Bound) - 1)); });

	switch (R)
	{
	case aquarium::BeginResult::InvalidNickname: return EBeginSessionResult::InvalidNickname;
	case aquarium::BeginResult::EmptyCatalog:    return EBeginSessionResult::EmptyCatalog;
	case aquarium::BeginResult::AlreadyActive:   return EBeginSessionResult::AlreadyActive;
	case aquarium::BeginResult::Ok:              break;
	}

	const int32 CatalogIndex = LoadedCatalogIndices[static_cast<int32>(Session.OwnedFishIndex())];
	PlayerFishActor = SpawnPlayerFish(Catalog[CatalogIndex].Mesh.Get());
	return EBeginSessionResult::Ok;
}

AFishActor* AAquariumGameMode::SpawnPlayerFish(USkeletalMesh* Mesh)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFishActor* Fish = GetWorld()->SpawnActor<AFishActor>(AFishActor::StaticClass(), PlaneOrigin, FRotator::ZeroRotator, Params);
	if (Fish == nullptr)
	{
		return nullptr;
	}
	Fish->bIsPlayerFish = true;
	Fish->Seed = static_cast<uint32>(Rng.RandRange(1, 1000000));
	Fish->PlaneOrigin = PlaneOrigin;
	Fish->PlaneHalfWidth = PlaneHalfWidth;
	Fish->PlaneHalfHeight = PlaneHalfHeight;
	Fish->FishMesh = Mesh;
	Fish->InitializeSwim();
	return Fish;
}

void AAquariumGameMode::EndSession()
{
	if (PlayerFishActor != nullptr)
	{
		PlayerFishActor->Destroy();
		PlayerFishActor = nullptr;
	}
	Session.End();
}

bool AAquariumGameMode::HasActiveSession() const
{
	return Session.HasActiveSession();
}

FString AAquariumGameMode::CurrentNickname() const
{
	return FString(UTF8_TO_TCHAR(Session.Nickname().c_str()));
}

int32 AAquariumGameMode::AssignedSpeciesIndex() const
{
	if (!Session.HasActiveSession())
	{
		return INDEX_NONE;
	}
	return LoadedCatalogIndices[static_cast<int32>(Session.OwnedFishIndex())];
}

AFishActor* AAquariumGameMode::PlayerFish() const
{
	return PlayerFishActor;
}
