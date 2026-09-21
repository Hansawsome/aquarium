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

	// 빠르게 지나간 자리에 남는 하얀 자국. **위로 뜨지 않는다.** 기포와 반대인 것이
	// 요점이다 -- 뜨면 그것은 시나리오가 이름 붙여 금지한 '귀여운 방울'이 된다.
	// 기포는 화면 위를 넘어야만 사라지고, 자국은 제 수명으로 그 자리에서 사라진다.
	void SpawnWake(const FVector& WorldPoint, const FVector2D& HeadingShared, uint32 Seed);
	int32 ActiveWakeCount() const { return static_cast<int32>(Wakes.size()); }
	int32 WakeSpawnedTotal() const { return WakeSpawnedTotalValue; }
	float HighestWakeZ() const;
	// 내 물고기가 이 속도 비율을 넘으면 자국을 남긴다. 최대 속도에서 **파생**한다 --
	// 리터럴 cm/s를 적으면 최대 속도를 조정할 때 조용히 어긋난다.
	float WakeSpeedFraction = 0.55f;
	float WakeInterval = 0.06f;    // 초. 자국 하나 사이의 간격
	float WakeLifetime = 0.5f;     // 초. 시간으로 사라지는 유일한 것이다
	AInstancedFieldActor* WakeField() const { return WakeFieldActor; }

private:
	std::vector<aquarium::Bubble> Bubbles;
	aquarium::BubbleParams BubbleParamsValue;
	UPROPERTY() TObjectPtr<AInstancedFieldActor> FieldActor = nullptr;
	float TopZValue = 400.f;
	int32 SpawnedTotalValue = 0;
	// 난류 자국 하나. 기포와 달리 뜨지 않으므로 riseSpeed가 없고, 대신 수명이 있다.
	struct FWake
	{
		FVector Location = FVector::ZeroVector;
		FVector2D Heading = FVector2D(1.f, 0.f);   // 공유 프레임
		float Age = 0.f;
		float Size = 0.f;
	};
	std::vector<FWake> Wakes;
	UPROPERTY() TObjectPtr<AInstancedFieldActor> WakeFieldActor = nullptr;
	int32 WakeSpawnedTotalValue = 0;
	float WakeTimer = 0.f;
	uint32 NextWakeSeed = 1u;
	void EnsureWakeField();
	void TickWakes(float DeltaTime);
	uint32 NextSeed = 1u;
	void EnsureField();
};
