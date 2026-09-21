#include "BubbleSubsystem.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "InstancedFieldActor.h"

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

void UBubbleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
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
