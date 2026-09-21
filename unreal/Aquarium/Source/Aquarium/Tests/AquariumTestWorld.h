#pragma once

// M8 테스트들이 공유하는 월드 헬퍼.
//
// **계획서는 `FAquariumTestWorld`와 `SpawnPlayerFishForTest` 따위가 이미 있다고
// 적었지만 이 저장소에는 없었다.** 기존 테스트들은 저마다 `CreateNewMap()` +
// `SpawnActor`를 손으로 반복하고 있었다. Task 7~12이 전부 "내 물고기 + 게임 모드"를
// 필요로 하므로, 같은 코드를 여섯 번 베끼는 대신(규약 8) 여기 한 번만 둔다.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AquariumGameMode.h"
#include "DiverPlayerController.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "FishActor.h"
#include "FishSpecies.h"
#include "Tests/AutomationEditorCommon.h"

namespace AquariumTest
{

// 새 맵 + 게임 모드. **GetAuthGameMode가 찾을 수 있도록 AuthorityGameMode에 직접
// 꽂는다** -- SpawnActor만으로는 월드가 그 액터를 권위 게임 모드로 인정하지 않아서
// ADiverPlayerController::GameMode()가 nullptr을 돌려주고, 그러면 컨트롤러를 통과하는
// 테스트가 전부 "아무것도 검증하지 않으면서 통과"한다(규약 6).
struct FWorld
{
	UWorld* World = nullptr;
	AAquariumGameMode* GM = nullptr;

	FWorld()
	{
		World = FAutomationEditorCommonUtils::CreateNewMap();
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GM = World->SpawnActor<AAquariumGameMode>(AAquariumGameMode::StaticClass(), FTransform::Identity, P);
		TArray<FFishSpecies> Catalog;
		FFishSpecies S;
		S.DisplayName = FText::FromString(TEXT("블루탱"));
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang")));
		Catalog.Add(S);
		GM->SetCatalogForTest(Catalog, 7);
		// AuthorityGameMode는 private이므로 그것을 쓰는 유일한 공개 경로로 꽂는다.
		World->CopyGameState(GM, World->GetGameState());
	}

	UWorld* Get() const { return World; }
	AAquariumGameMode* GameMode() const { return GM; }

	// 세션을 열고 내 물고기를 돌려준다. nullptr이면 호출자가 즉시 실패해야 한다.
	AFishActor* BeginSession(const TCHAR* Nickname = TEXT("민지"))
	{
		GM->BeginSession(FString(Nickname));
		return GM->PlayerFish();
	}
};

// 배경 물고기 한 마리. 내 물고기보다 **뒤쪽 평면**(X >= 330)에 둔다 -- 이 프로젝트의
// 실제 배치와 같고, 잡기 판정이 카메라에서 본 겹침이라는 전제가 여기 걸려 있다.
inline AFishActor* SpawnBackgroundFish(UWorld* World, uint32 Seed, float PlaneX = 400.f,
                                       const FVector2D& PlaneYZ = FVector2D(0.f, 100.f))
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFishActor* Fish = World->SpawnActor<AFishActor>(AFishActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	Fish->Seed = Seed;
	Fish->PlaneOrigin = FVector(PlaneX, PlaneYZ.X, PlaneYZ.Y);
	Fish->PlaneHalfWidth = 300.f;
	Fish->PlaneHalfHeight = 150.f;
	if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang")))
	{
		Fish->SetMesh(Mesh);
	}
	Fish->InitializeSwim();
	return Fish;
}

} // namespace AquariumTest

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
