#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <vector>

#include "aquarium/Bubbles.h"

#include "BubbleSubsystem.generated.h"

class AInstancedFieldActor;

// 이 게임의 모든 기포. 클릭 한 번에 한 줄기씩 생기고, **화면 위를 넘을 때만**
// 사라진다(시나리오 장면 2 요구사항 3 — 시선의 약속). 액터 틱은 0개다.
UCLASS()
class AQUARIUM_API UBubbleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBubbleSubsystem, STATGROUP_Tickables); }

	// 한 줄기(Count개)를 월드 지점에서 띄운다. Seed가 속도·크기·흔들림을 정한다.
	// TopZ는 **이 줄기**가 사라지는 높이다. 깊은 평면일수록 화면 위 끝이 높아서
	// 전체에 하나의 높이를 쓸 수 없다(원근). 생략하면 SetTopZ의 값을 쓴다.
	void Spawn(const FVector& WorldPoint, int32 Count, uint32 Seed);
	void Spawn(const FVector& WorldPoint, int32 Count, uint32 Seed, float TopZForThisBurst);
	int32 ActiveCount() const { return static_cast<int32>(Bubbles.size()); }
	int32 SpawnedTotal() const { return SpawnedTotalValue; }
	// 살아 있는 기포 중 가장 높은 월드 Z. 테스트가 '정말 위까지 올라갔다'를 본다.
	float HighestZ() const;

	// 기포가 사라지는 높이(월드 Z). 물고기 유영 영역의 위 끝에서 **파생**한다.
	// 여기에 리터럴을 쓰면 M3의 화면 비율 대응이 조용히 깨진다.
	float TopZ() const { return TopZValue; }
	void SetTopZ(float InTopZ) { TopZValue = InTopZ; }

	AInstancedFieldActor* Field() const { return FieldActor; }

private:
	std::vector<aquarium::Bubble> Bubbles;
	aquarium::BubbleParams BubbleParamsValue;
	UPROPERTY() TObjectPtr<AInstancedFieldActor> FieldActor = nullptr;
	float TopZValue = 400.f;
	int32 SpawnedTotalValue = 0;
	uint32 NextSeed = 1u;
	void EnsureField();
};
