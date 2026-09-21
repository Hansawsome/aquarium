#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <vector>

#include "aquarium/Catch.h"
#include "aquarium/Evade.h"
#include "aquarium/Impact.h"
#include "aquarium/Stamp.h"

#include "CatchSubsystem.generated.h"

class AFishActor;
class ACameraActor;

// 부딪혀 잡기의 판정과 결과를 모두 가진 곳. **판정은 조향이 아니라 관측이므로
// 규칙 순서 바깥이다** -- 모든 물고기가 한 틱을 다 움직인 뒤에 읽기만 한다.
// 액터 틱은 하나도 늘지 않는다(서브시스템 하나가 전부 훑는다).
UCLASS()
class AQUARIUM_API UCatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCatchSubsystem, STATGROUP_Tickables); }

	// 화면 구석의 숫자. 오르기만 한다.
	int32 StampCount() const { return Book.Count(); }
	// 이 바다에 있는 잡을 수 있는 물고기 수. 36을 리터럴로 쓰지 않기 위한 파생값이다.
	int32 CatchableCount() const;
	// 나가기. 장부를 비우고 모든 도장을 떼어 낸다(한 마리씩이 아니라 통째로).
	void ResetSession();

	// 부딪힌 순간의 화면 흔들림. 카메라를 실제로 흔드는 일은 Task 12가 한다.
	const aquarium::ImpactShake& Shake() const { return ShakeValue; }
	const aquarium::ImpactParams& ShakeParams() const { return ImpactParamsValue; }

	// 개발 전용: 판정을 통째로 끈다(성능 귀속). 값을 받지 않는 불리언이다.
	bool bCatchEnabled = true;

	// --- 테스트 전용 ---
	// 내 물고기 코끝의 화면 위치에 배경 물고기 한 마리를 놓는다.
	AFishActor* SpawnTargetUnderNoseForTest(AFishActor* Mine);
	void SetTargetUnderNoseForTest(AFishActor* Target, AFishActor* Mine);
	// 이미 있는 놈을 코끝 앞쪽 ScreenGap만큼 떨어진 자리로 옮긴다.
	void SetTargetAheadForTest(AFishActor* Target, AFishActor* Mine, float ScreenGap);
	// 코끝 앞쪽 ScreenGap만큼 떨어진 자리에 놓는다(회피 시험용).
	AFishActor* SpawnTargetAheadForTest(AFishActor* Mine, float ScreenGap);
	// 흔들림만 발화시킨다(Task 12).
	void TriggerShakeForTest() { ShakeValue.Hit(ImpactParamsValue); }

private:
	aquarium::StampBook Book;
	aquarium::CatchParams CatchParamsValue;
	aquarium::EvadeParams EvadeParamsValue;
	aquarium::ImpactParams ImpactParamsValue;
	aquarium::ImpactShake ShakeValue;
	std::vector<aquarium::RamTarget> Targets;
	TArray<TWeakObjectPtr<AFishActor>> TargetActors;
	UPROPERTY() TObjectPtr<ACameraActor> Camera = nullptr;
	// 카메라 위치. DiverCamera 태그로 한 번만 찾는다(게임 모드와 같은 방식).
	FVector CameraLocation();
	void OnCaught(AFishActor* Fish);
	void OnBumped(AFishActor* Fish, AFishActor* Mine);
	void NoticeNearby(AFishActor* Mine, const aquarium::Vec2& MyScreen, const aquarium::Vec2& MyScreenVel);
	// 테스트가 배경 물고기를 코끝 위/앞에 놓을 때 공유하는 계산.
	void PlaceTargetAtScreenOffset(AFishActor* Target, AFishActor* Mine, float ScreenGap);
	uint32 NextCueSeed = 1u;
};
