#include "CatchSubsystem.h"

#include "AquariumAudioSubsystem.h"
#include "AquariumGameMode.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "FishActor.h"
#include "FishSchoolSubsystem.h"
#include "Kismet/GameplayStatics.h"

FVector UCatchSubsystem::CameraLocation()
{
	if (Camera == nullptr)
	{
		TArray<AActor*> Cameras;
		UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ACameraActor::StaticClass(),
			FName(TEXT("DiverCamera")), Cameras);
		if (Cameras.Num() > 0)
		{
			Camera = Cast<ACameraActor>(Cameras[0]);
		}
	}
	// 카메라가 없는 월드(자동화 맵)에서는 원점이다. 규칙 계층 테스트가 쓰는 것과
	// 같은 기준이라 두 계층이 같은 그림을 본다.
	return Camera ? Camera->GetActorLocation() : FVector::ZeroVector;
}

int32 UCatchSubsystem::CatchableCount() const
{
	UWorld* W = GetWorld();
	UFishSchoolSubsystem* School = W ? W->GetSubsystem<UFishSchoolSubsystem>() : nullptr;
	AAquariumGameMode* GM = W ? W->GetAuthGameMode<AAquariumGameMode>() : nullptr;
	AFishActor* Mine = GM ? GM->PlayerFish() : nullptr;
	if (School == nullptr) return 0;
	int32 Count = 0;
	for (const TWeakObjectPtr<AFishActor>& Weak : School->RegisteredFish())
	{
		AFishActor* Fish = Weak.Get();
		// 내 물고기는 잡을 수 없으므로 완주 조건에도 들어가지 않는다. 넣으면 36을
		// 다 찍어도 완주가 영원히 안 된다.
		if (Fish == nullptr || Fish == Mine || Fish->bIsPlayerFish) continue;
		++Count;
	}
	return Count;
}

void UCatchSubsystem::ResetSession()
{
	Book.Reset();
	UWorld* W = GetWorld();
	if (UFishSchoolSubsystem* School = W ? W->GetSubsystem<UFishSchoolSubsystem>() : nullptr)
	{
		for (const TWeakObjectPtr<AFishActor>& Weak : School->RegisteredFish())
		{
			if (AFishActor* Fish = Weak.Get())
			{
				Fish->ClearStampForSessionReset();
			}
		}
	}
}

void UCatchSubsystem::OnCaught(AFishActor* Fish)
{
	// 처음 찍은 것일 때만 숫자가 오른다. 두 번째부터는 아무 일도 일어나지 않는 것이
	// 아니라 -- 소리와 흔들림은 매번 난다. 부딪힌 것은 부딪힌 것이다.
	UWorld* W = GetWorld();
	AAquariumGameMode* GM = W ? W->GetAuthGameMode<AAquariumGameMode>() : nullptr;
	if (Book.Stamp(static_cast<int>(Fish->GetUniqueID())))
	{
		// 별명은 메모리에서 위젯으로 바로 간다. 이 경로 어디에도 로그가 없다(M6).
		Fish->ApplyStamp(FText::FromString(GM ? GM->CurrentNickname() : FString()));
	}
	if (UAquariumAudioSubsystem* Audio = W ? W->GetSubsystem<UAquariumAudioSubsystem>() : nullptr)
	{
		Audio->PlayCue(EAquariumCue::Thud, NextCueSeed++);
	}
	ShakeValue.Hit(ImpactParamsValue);
}

void UCatchSubsystem::OnBumped(AFishActor* Fish, AFishActor* Mine)
{
	// **벌은 없다.** 놈이 놀라서 튀고, 그것이 다시 붙을 이유가 된다.
	Fish->ApplyFleeFrom(Mine->GetActorLocation());
	if (UAquariumAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UAquariumAudioSubsystem>() : nullptr)
	{
		Audio->PlayCue(EAquariumCue::Startle, NextCueSeed++);
	}
}

void UCatchSubsystem::NoticeNearby(AFishActor* Mine, const aquarium::Vec2& MyScreen,
                                   const aquarium::Vec2& MyScreenVel)
{
	const FVector Cam = CameraLocation();
	const aquarium::Vec3 Camera3{static_cast<float>(Cam.X), static_cast<float>(Cam.Y), static_cast<float>(Cam.Z)};
	aquarium::Approach A;
	A.screenPos = MyScreen;
	A.screenVel = MyScreenVel;
	for (int32 i = 0; i < TargetActors.Num(); ++i)
	{
		AFishActor* Fish = TargetActors[i].Get();
		if (Fish == nullptr) continue;
		const aquarium::RamTarget& T = Targets[static_cast<size_t>(i)];
		const aquarium::Vec2 ItsScreen = aquarium::ToScreen(T.center, T.depth, Camera3);
		if (!aquarium::ShouldNotice(ItsScreen, A, EvadeParamsValue)) continue;
		// 접근선의 방향. 화면 단위지만 방향만 쓰므로 축척은 상관없다 -- 회피는
		// 이 선의 **수직**을 고르는 일이고 수직은 균일 축척에 불변이다.
		const aquarium::Vec2 Dir = (ItsScreen - MyScreen).Normalized();
		Fish->NoticeApproach(Dir);
	}
}

void UCatchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ShakeValue.Step(DeltaTime);
	if (!bCatchEnabled || DeltaTime <= 0.f) return;

	UWorld* W = GetWorld();
	AAquariumGameMode* GM = W ? W->GetAuthGameMode<AAquariumGameMode>() : nullptr;
	AFishActor* Mine = GM ? GM->PlayerFish() : nullptr;
	UFishSchoolSubsystem* School = W ? W->GetSubsystem<UFishSchoolSubsystem>() : nullptr;
	if (Mine == nullptr || School == nullptr || Mine->IsPaused()) return;

	const FVector Cam = CameraLocation();
	const aquarium::Vec3 Camera3{static_cast<float>(Cam.X), static_cast<float>(Cam.Y), static_cast<float>(Cam.Z)};

	Targets.clear();
	TargetActors.Reset();
	for (const TWeakObjectPtr<AFishActor>& Weak : School->RegisteredFish())
	{
		AFishActor* Fish = Weak.Get();
		// 내 물고기는 자기를 잡지 않는다. 이것이 없으면 첫 틱에 스스로를 찍는다.
		if (Fish == nullptr || Fish == Mine || Fish->bIsPlayerFish) continue;
		Targets.push_back(Fish->AsRamTarget());
		TargetActors.Add(Fish);
	}
	if (Targets.empty()) return;

	aquarium::Rammer Me;
	const aquarium::ClickTarget MyBody = Mine->AsClickTarget();
	Me.depth = MyBody.depth;
	Me.nose = Mine->NosePoint();
	Me.velocity = Mine->SwimVelocity();
	// **반드시 채운다.** 0이면 잡기 문턱이 0이 되어 "느리게 표류해도 잡힌다"가 된다.
	Me.maxSpeed = Mine->MaxSpeed;

	// 1) 먼저 눈치채게 한다. 판정보다 **앞**이라야 "놈이 먼저 눈치챘다"가 성립한다.
	NoticeNearby(Mine, aquarium::ToScreen(Me.nose, Me.depth, Camera3),
	             aquarium::ToScreenVelocity(Me.velocity, Me.depth, Camera3));

	// 2) 판정.
	const aquarium::RamResult R = aquarium::EvaluateRam(Camera3, Me, Targets.data(), Targets.size(), CatchParamsValue);
	if (R.targetIndex < 0) return;
	AFishActor* Hit = TargetActors[R.targetIndex].Get();
	if (Hit == nullptr) return;
	if (R.outcome == aquarium::RamOutcome::Catch) { ++CatchTickCount; OnCaught(Hit); }
	else { ++BumpTickCount; OnBumped(Hit, Mine); }
#if !UE_BUILD_SHIPPING
	// 난이도 계측의 **유일한** 기구. 시나리오 181행이 "잡기가 실제로 어려운지는
	// 테스트로 알 수 없다"고 못 박았으므로, 실제 RHI 실행의 로그에서 세는 이 줄이
	// 난이도 판정의 근거다. 별명은 어디에도 들어가지 않는다(P-03).
	UE_LOG(LogTemp, Warning,
		TEXT("AquariumCatchStat: t=%.2f outcome=%s stamped=%d catches=%d bumps=%d closing=%.2f threshold=%.2f"),
		W->GetTimeSeconds(),
		R.outcome == aquarium::RamOutcome::Catch ? TEXT("catch") : TEXT("bump"),
		Book.Count(), CatchTickCount, BumpTickCount, R.closingSpeed,
		aquarium::CatchThreshold(Me.maxSpeed, Me.depth, CatchParamsValue));
#endif
}

// ---------------------------------------------------------------- 테스트 전용

void UCatchSubsystem::PlaceTargetAtScreenOffset(AFishActor* Target, AFishActor* Mine, float ScreenGap)
{
	const FVector Cam = CameraLocation();
	const aquarium::Vec3 Camera3{static_cast<float>(Cam.X), static_cast<float>(Cam.Y), static_cast<float>(Cam.Z)};
	const aquarium::ClickTarget MyBody = Mine->AsClickTarget();
	const aquarium::Vec2 Nose = Mine->NosePoint();
	aquarium::Vec2 Screen = aquarium::ToScreen(Nose, MyBody.depth, Camera3);
	if (ScreenGap != 0.f)
	{
		aquarium::Vec2 Dir = aquarium::ToScreenVelocity(Mine->SwimVelocity(), MyBody.depth, Camera3).Normalized();
		if (Dir.Length() <= 0.f) Dir = {1.f, 0.f};
		Screen = Screen + Dir * ScreenGap;
	}
	// 화면 좌표를 그 깊이의 월드 좌표로 되돌린다. ToScreen의 역이다.
	const float D = static_cast<float>(Target->PlaneOrigin.X) - Camera3.x;
	Target->PlaneOrigin = FVector(Target->PlaneOrigin.X,
		static_cast<double>(Camera3.y + Screen.x * D),
		static_cast<double>(Camera3.z + Screen.y * D));
	Target->InitializeSwim();
}

AFishActor* UCatchSubsystem::SpawnTargetUnderNoseForTest(AFishActor* Mine)
{
	return SpawnTargetAheadForTest(Mine, 0.f);
}

void UCatchSubsystem::SetTargetUnderNoseForTest(AFishActor* Target, AFishActor* Mine)
{
	PlaceTargetAtScreenOffset(Target, Mine, 0.f);
}

void UCatchSubsystem::SetTargetAheadForTest(AFishActor* Target, AFishActor* Mine, float ScreenGap)
{
	PlaceTargetAtScreenOffset(Target, Mine, ScreenGap);
}

AFishActor* UCatchSubsystem::SpawnTargetAheadForTest(AFishActor* Mine, float ScreenGap)
{
	UWorld* W = GetWorld();
	if (W == nullptr || Mine == nullptr) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFishActor* Target = W->SpawnActor<AFishActor>(AFishActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	Target->Seed = 4242u;
	// 배경 물고기의 실제 평면대로 내 물고기(220)보다 뒤다.
	Target->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	Target->PlaneHalfWidth = 300.f;
	Target->PlaneHalfHeight = 150.f;
	if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang")))
	{
		Target->SetMesh(Mesh);
	}
	Target->InitializeSwim();
	PlaceTargetAtScreenOffset(Target, Mine, ScreenGap);
	if (UFishSchoolSubsystem* School = W->GetSubsystem<UFishSchoolSubsystem>())
	{
		School->Register(Target);
	}
	return Target;
}
