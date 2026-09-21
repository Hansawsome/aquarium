#include "BubbleSubsystem.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "InstancedFieldActor.h"

#include "AquariumGameMode.h"
#include "FishActor.h"

void UBubbleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Bubbles.clear();
}

void UBubbleSubsystem::EnsureField()
{
	if (FieldActor != nullptr)
	{
		return;
	}
	UWorld* W = GetWorld();
	if (W == nullptr)
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FieldActor = W->SpawnActor<AInstancedFieldActor>(AInstancedFieldActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (FieldActor)
	{
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fx/M_Bubble.M_Bubble"));
		FieldActor->Configure(Sphere, Mat);
	}
}

void UBubbleSubsystem::Spawn(const FVector& WorldPoint, int32 Count, uint32 Seed)
{
	Spawn(WorldPoint, Count, Seed, TopZValue);
}

void UBubbleSubsystem::Spawn(const FVector& WorldPoint, int32 Count, uint32 Seed, float TopZForThisBurst)
{
	// 개수에 상한을 두지 않는다. 시나리오가 금지한 것은 아이가 느끼는 제한이고,
	// 기포는 화면 위를 넘으면 스스로 사라지므로 상한 없이도 유한하다.
	EnsureField();
	for (int32 i = 0; i < Count; ++i)
	{
		const uint32 S = Seed * 131u + static_cast<uint32>(i) * 17u + NextSeed++;
		aquarium::Bubble B = aquarium::MakeBubble(
			{static_cast<float>(WorldPoint.Y), static_cast<float>(WorldPoint.Z)},
			static_cast<float>(WorldPoint.X), S, BubbleParamsValue);
		B.topY = TopZForThisBurst;
		Bubbles.push_back(B);
		++SpawnedTotalValue;
	}
}

float UBubbleSubsystem::HighestZ() const
{
	float Best = -1e9f;
	for (const aquarium::Bubble& B : Bubbles) { Best = FMath::Max(Best, B.position.y); }
	return Best;
}

void UBubbleSubsystem::EnsureWakeField()
{
	if (WakeFieldActor != nullptr) return;
	UWorld* W = GetWorld();
	if (W == nullptr) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	WakeFieldActor = W->SpawnActor<AInstancedFieldActor>(AInstancedFieldActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (WakeFieldActor)
	{
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Fx/M_Wake.M_Wake"));
		WakeFieldActor->Configure(Sphere, Mat);
	}
}

void UBubbleSubsystem::SpawnWake(const FVector& WorldPoint, const FVector2D& HeadingShared, uint32 Seed)
{
	EnsureWakeField();
	FWake Wk;
	Wk.Location = WorldPoint;
	const FVector2D H = HeadingShared.GetSafeNormal();
	Wk.Heading = H.IsNearlyZero() ? FVector2D(1.f, 0.f) : H;
	// 크기에 약간의 흔들림. 같은 크기가 줄줄이 나오면 그것은 난류가 아니라 점선이다.
	Wk.Size = 5.5f * (0.8f + 0.4f * aquarium::BubbleUnit(Seed * 7u + 13u));
	Wakes.push_back(Wk);
	++WakeSpawnedTotalValue;
}

float UBubbleSubsystem::HighestWakeZ() const
{
	float Best = -1e9f;
	for (const FWake& Wk : Wakes) { Best = FMath::Max(Best, static_cast<float>(Wk.Location.Z)); }
	return Best;
}

void UBubbleSubsystem::TickWakes(float DeltaTime)
{
	// 내 물고기가 충분히 빠를 때만 자국이 난다. 표류에도 자국이 나면 그것은
	// '빠르게 지나간 자리'라는 뜻을 잃는다.
	UWorld* W = GetWorld();
	AAquariumGameMode* GM = W ? W->GetAuthGameMode<AAquariumGameMode>() : nullptr;
	if (AFishActor* Mine = GM ? GM->PlayerFish() : nullptr)
	{
		const float Threshold = Mine->MaxSpeed * WakeSpeedFraction;
		if (!Mine->IsPaused() && Mine->CurrentSpeed() > Threshold)
		{
			WakeTimer += DeltaTime;
			while (WakeTimer >= WakeInterval && WakeInterval > 1e-4f)
			{
				WakeTimer -= WakeInterval;
				const aquarium::Vec2 V = Mine->SwimVelocity();
				SpawnWake(Mine->GetActorLocation(), FVector2D(V.x, V.y), NextWakeSeed++);
			}
		}
		else
		{
			WakeTimer = 0.f;
		}
	}

	if (Wakes.empty())
	{
		if (WakeFieldActor && WakeFieldActor->InstanceCount() > 0) { WakeFieldActor->UpdateInstances({}); }
		return;
	}
	TArray<FTransform> Transforms;
	Transforms.Reserve(static_cast<int32>(Wakes.size()));
	size_t write = 0;
	for (size_t i = 0; i < Wakes.size(); ++i)
	{
		FWake Wk = Wakes[i];
		Wk.Age += DeltaTime;
		// **위치는 한 줄도 바뀌지 않는다.** 뜨지도 흐르지도 않는다 -- 지나간 자리에
		// 남는 흔적이지 떠다니는 방울이 아니기 때문이다.
		if (Wk.Age >= WakeLifetime) continue;
		Wakes[write++] = Wk;
		const float Fade = 1.f - (Wk.Age / WakeLifetime);
		// 가늘고 길게 늘어난 모양. 진행 방향으로 3배, 수직으로 0.4배.
		// 시간에 따라 **줄어들며** 사라진다(반대로 부풀리는 것은 금지 목록이다).
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Wk.Heading.Y, Wk.Heading.X));
		const FQuat Rot(FVector::ForwardVector, FMath::DegreesToRadians(AngleDeg));
		const float S = (Wk.Size * Fade) / 50.f;   // 엔진 구 메시는 반지름 50 cm다
		Transforms.Add(FTransform(Rot, Wk.Location, FVector(S * 0.4f, S * 3.f, S * 0.4f)));
	}
	Wakes.resize(write);
	EnsureWakeField();
	if (WakeFieldActor) { WakeFieldActor->UpdateInstances(Transforms); }
}

void UBubbleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickWakes(DeltaTime);
	if (Bubbles.empty())
	{
		if (FieldActor && FieldActor->InstanceCount() > 0) { FieldActor->UpdateInstances({}); }
		return;
	}
	TArray<FTransform> Transforms;
	Transforms.Reserve(static_cast<int32>(Bubbles.size()));
	size_t write = 0;
	for (size_t i = 0; i < Bubbles.size(); ++i)
	{
		aquarium::Bubble B = Bubbles[i];
		aquarium::StepBubble(B, DeltaTime, BubbleParamsValue);
		// 지우는 조건은 높이 하나뿐이다. 수명으로 지우면 아이가 알아챈다.
		// 기포마다 자기 높이로 판정한다. 공유 높이를 쓰면 깊은 평면의 기포가
		// 화면 한가운데에서 사라진다.
		if (aquarium::BubbleIsGone(B))
		{
			continue;
		}
		Bubbles[write++] = B;
		Transforms.Add(FTransform(FQuat::Identity,
			FVector(B.depth, B.position.x, B.position.y),
			FVector(B.radius / 50.f)));   // 엔진 구 메시는 반지름 50 cm다
	}
	Bubbles.resize(write);
	EnsureField();
	if (FieldActor) { FieldActor->UpdateInstances(Transforms); }
}
