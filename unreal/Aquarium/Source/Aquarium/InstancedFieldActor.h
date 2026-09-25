#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "InstancedFieldActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

// 작은 것 여러 개를 **틱 없이** 그리는 액터. 기포와 먹이가 각각 하나씩 쓴다.
//
// 액터당 틱을 만들지 않는 것이 요구사항이다(먹이에는 개수 제한이 없어야 하므로
// 아이가 화면을 하얗게 덮을 때까지 뿌릴 수 있어야 한다). 이 액터는 상태를 갖지
// 않고 시뮬레이션도 하지 않는다 -- 서브시스템이 계산한 위치를 받아 인스턴스
// 변환만 통째로 갈아끼운다.
UCLASS()
class AQUARIUM_API AInstancedFieldActor : public AActor
{
	GENERATED_BODY()

public:
	AInstancedFieldActor();

	// 엔진 기본 구 메시 + 지정한 머티리얼로 컴포넌트를 준비한다. 두 번 불러도 안전하다.
	void Configure(UStaticMesh* Mesh, UMaterialInterface* Material);
	// 이번 프레임의 인스턴스 전체. 개수가 바뀌어도 한 번의 배치 갱신으로 끝난다.
	void UpdateInstances(const TArray<FTransform>& Transforms);
	int32 InstanceCount() const;
	UInstancedStaticMeshComponent* Instances() const { return Mesh; }

private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Mesh = nullptr;
};
