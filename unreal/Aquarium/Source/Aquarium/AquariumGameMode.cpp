#include "AquariumGameMode.h"

#include "CatchSubsystem.h"
#include "DiverPlayerController.h"
#include "DiverSpectatorPawn.h"
#include "FishActor.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"

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

	FFishSpecies YellowTang;
	YellowTang.DisplayName = NSLOCTEXT("Aquarium", "SpeciesYellowTang", "노란탱");
	YellowTang.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/YellowTang/SK_YellowTang.SK_YellowTang")));
	Catalog.Add(YellowTang);

	FFishSpecies Butterflyfish;
	Butterflyfish.DisplayName = NSLOCTEXT("Aquarium", "SpeciesButterflyfish", "나비고기");
	Butterflyfish.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/Butterflyfish/SK_Butterflyfish.SK_Butterflyfish")));
	Catalog.Add(Butterflyfish);

	FFishSpecies Damselfish;
	Damselfish.DisplayName = NSLOCTEXT("Aquarium", "SpeciesDamselfish", "담셀피시");
	Damselfish.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/Damselfish/SK_Damselfish.SK_Damselfish")));
	Catalog.Add(Damselfish);
}

void AAquariumGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Cycles() varies per launch even when several instances start within the same clock tick.
	const int32 Seed = AssignmentSeed != 0 ? AssignmentSeed : static_cast<int32>(FPlatformTime::Cycles());
	Rng.Initialize(Seed);
	RebuildLoadedCatalog();
}

void AAquariumGameMode::SetCatalogForTest(const TArray<FFishSpecies>& InCatalog, int32 Seed)
{
	ensureMsgf(!Session.HasActiveSession(), TEXT("AquariumGameMode: catalog changed while a session is active"));
	EndSession();
	Catalog = InCatalog;
	AssignmentSeed = Seed;
	Rng.Initialize(Seed);
	RebuildLoadedCatalog();
}

bool AAquariumGameMode::SetAssignmentSeed(int32 Seed)
{
	if (Seed == 0 || Session.HasActiveSession())
	{
		return false;
	}
	AssignmentSeed = Seed;
	Rng.Initialize(Seed);
	return true;
}

void AAquariumGameMode::RebuildLoadedCatalog()
{
	LoadedCatalogIndices.Reset();
	LoadedMeshes.Reset();
	for (int32 i = 0; i < Catalog.Num(); ++i)
	{
		// TODO: switch to FStreamableManager async loading if the catalog grows beyond a few meshes.
		if (USkeletalMesh* Mesh = Catalog[i].Mesh.LoadSynchronous())
		{
			LoadedCatalogIndices.Add(i);
			LoadedMeshes.Add(Mesh);
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

	FitSwimPlaneToViewport();
	PlayerFishActor = SpawnPlayerFish(LoadedMeshes[static_cast<int32>(Session.OwnedFishIndex())]);
	if (PlayerFishActor == nullptr)
	{
		// Fail closed so HasActiveSession() and PlayerFish() never disagree.
		Session.End();
		UE_LOG(LogTemp, Warning, TEXT("AquariumGameMode: player fish spawn failed; session not started"));
		return EBeginSessionResult::EmptyCatalog;
	}
	// Name tag (F-04): the nickname goes straight from memory into the widget; never log it.
	PlayerFishActor->AttachNameTag(FText::FromString(CurrentNickname()));
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
	// F-07: the session fish is driven by the arrow keys, not by its wander behavior.
	Fish->bPlayerControlled = true;
	Fish->MaxSpeed = PlayerMaxSpeed;
	Fish->Accel = PlayerAccel;
	Fish->Decel = PlayerDecel;
	Fish->Seed = static_cast<uint32>(Rng.RandRange(1, 1000000));
	Fish->PlaneOrigin = PlaneOrigin;
	Fish->PlaneHalfWidth = PlaneHalfWidth;
	Fish->PlaneHalfHeight = PlaneHalfHeight;
	Fish->FishMesh = Mesh;
	Fish->InitializeSwim();

	// Normalize the body length so every species reads the same size (see PlayerFishTargetLengthCm).
	// GetBounds() is the skeletal mesh asset's own bounds in cm; X is the nose-to-tail axis.
	const float NativeLength = Mesh != nullptr ? Mesh->GetBounds().BoxExtent.X * 2.f : 0.f;
	if (FMath::IsFinite(NativeLength) && NativeLength > KINDA_SMALL_NUMBER)
	{
		const float Scale = FMath::Clamp(PlayerFishTargetLengthCm / NativeLength, 0.5f, 5.f);
		Fish->SetActorScale3D(FVector(Scale));
	}
	else
	{
		// Never log the nickname; the mesh path is enough to find the bad asset.
		UE_LOG(LogTemp, Warning, TEXT("AquariumGameMode: mesh '%s' has no usable bounds; player fish left at scale 1"),
			Mesh != nullptr ? *Mesh->GetPathName() : TEXT("<null>"));
	}
	return Fish;
}

FVector2D AAquariumGameMode::VisibleHalfExtents(float DistanceCm, float HorizontalFovDeg, float AspectRatio)
{
	if (DistanceCm <= KINDA_SMALL_NUMBER || HorizontalFovDeg <= 1.f || HorizontalFovDeg >= 179.f
		|| AspectRatio <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}
	const float HalfWidth = DistanceCm * FMath::Tan(FMath::DegreesToRadians(HorizontalFovDeg) * 0.5f);
	return FVector2D(HalfWidth, HalfWidth / AspectRatio);
}

float AAquariumGameMode::ScreenTopZAt(float DepthCm) const
{
	// 화면 위 끝보다 조금 더 위에서 지운다 -- 가장자리에서 툭 사라지면 아이가
	// 알아챈다(시나리오 장면 2 요구사항 3, 시선의 약속).
	constexpr float kAboveTheEdge = 1.10f;
	const FVector2D Visible = VisibleHalfExtents(DepthCm, ViewFovDeg, ViewAspect);
	const float Half = (Visible.Y > 0.f) ? Visible.Y : PlaneHalfHeight;
	return static_cast<float>(PlaneOrigin.Z) + Half * kAboveTheEdge;
}

FVector2D AAquariumGameMode::FitPlaneToView(float DistanceCm, float HorizontalFovDeg, float AspectRatio, FVector2D RequestedHalfExtents)
{
	// Keep the fish off the very edge of the frame; the name tag sits above the body and would
	// otherwise be clipped at the top of a narrow viewport.
	constexpr float kInset = 0.92f;
	// Never smaller than this, so a strange viewport cannot collapse the plane to a point.
	constexpr float kMinHalfWidth = 40.f;
	constexpr float kMinHalfHeight = 20.f;

	FVector2D Fitted = RequestedHalfExtents;
	const FVector2D Visible = VisibleHalfExtents(DistanceCm, HorizontalFovDeg, AspectRatio);
	if (Visible.X > 0.f)
	{
		Fitted.X = FMath::Min(Fitted.X, Visible.X * kInset);
		Fitted.Y = FMath::Min(Fitted.Y, Visible.Y * kInset);
	}
	Fitted.X = FMath::Max(Fitted.X, kMinHalfWidth);
	Fitted.Y = FMath::Max(Fitted.Y, kMinHalfHeight);
	return Fitted;
}

void AAquariumGameMode::FitSwimPlaneToViewport()
{
	float Aspect = 16.f / 9.f;
	if (GEngine != nullptr && GEngine->GameViewport != nullptr && GEngine->GameViewport->Viewport != nullptr)
	{
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (Size.X > 0 && Size.Y > 0)
		{
			Aspect = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
		}
	}

	float Fov = 75.f;
	TArray<AActor*> Cameras;
	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ACameraActor::StaticClass(), FName(TEXT("DiverCamera")), Cameras);
	if (Cameras.Num() > 0)
	{
		if (const ACameraActor* Camera = Cast<ACameraActor>(Cameras[0]))
		{
			if (const UCameraComponent* Component = Camera->GetCameraComponent())
			{
				Fov = Component->FieldOfView;
			}
		}
	}

	// Always from the authored request, never from the current extents (see the header).
	ViewFovDeg = Fov;
	ViewAspect = Aspect;
	const FVector2D Fitted = FitPlaneToView(PlaneOrigin.X, Fov, Aspect, RequestedPlaneHalfExtents);
	PlaneHalfWidth = Fitted.X;
	PlaneHalfHeight = Fitted.Y;
	// Geometry only; the nickname is never logged.
	UE_LOG(LogTemp, Log, TEXT("AquariumGameMode: swim plane fitted to aspect %.3f, fov %.1f -> half extents (%.1f, %.1f)"),
		Aspect, Fov, PlaneHalfWidth, PlaneHalfHeight);
}

void AAquariumGameMode::EndSession()
{
	if (PlayerFishActor != nullptr)
	{
		PlayerFishActor->Destroy();
		PlayerFishActor = nullptr;
	}
	Session.End();
	// 나가기로 도장이 리셋된다(시나리오 결정표). 세션 동안은 유지된다.
	if (UWorld* W = GetWorld())
	{
		if (UCatchSubsystem* CatchSub = W->GetSubsystem<UCatchSubsystem>())
		{
			CatchSub->ResetSession();
		}
	}
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
