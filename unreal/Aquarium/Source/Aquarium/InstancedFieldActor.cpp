#include "InstancedFieldActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AInstancedFieldActor::AInstancedFieldActor()
{
	// 액터 틱 없음. 이것이 이 클래스의 존재 이유다.
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);                 // 알갱이 수백 개의 그림자는 순전히 낭비다
	Mesh->SetMobility(EComponentMobility::Movable);
}

void AInstancedFieldActor::Configure(UStaticMesh* InMesh, UMaterialInterface* Material)
{
	if (Mesh == nullptr || InMesh == nullptr)
	{
		return;
	}
	Mesh->SetStaticMesh(InMesh);
	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
}

void AInstancedFieldActor::UpdateInstances(const TArray<FTransform>& Transforms)
{
	if (Mesh == nullptr)
	{
		return;
	}
	// 개수가 같으면 변환만 갈아끼우고, 다르면 통째로 다시 만든다. 한 프레임에
	// 인스턴스 하나씩 더하고 빼는 것보다 이쪽이 싸고, 무엇보다 상태가 없다.
	if (Mesh->GetInstanceCount() != Transforms.Num())
	{
		Mesh->ClearInstances();
		for (const FTransform& T : Transforms) { Mesh->AddInstance(T, /*bWorldSpace*/ true); }
		return;
	}
	for (int32 i = 0; i < Transforms.Num(); ++i)
	{
		Mesh->UpdateInstanceTransform(i, Transforms[i], /*bWorldSpace*/ true,
			/*bMarkRenderStateDirty*/ i == Transforms.Num() - 1, /*bTeleport*/ true);
	}
}

int32 AInstancedFieldActor::InstanceCount() const
{
	return Mesh ? Mesh->GetInstanceCount() : 0;
}
