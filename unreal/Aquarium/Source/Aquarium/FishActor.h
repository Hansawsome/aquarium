#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"

#include "aquarium/Bounds.h"
#include "aquarium/Heading.h"
#include "aquarium/Motion.h"
#include "aquarium/SwimAnimation.h"
#include "aquarium/SwimPlane.h"
#include "aquarium/Wander.h"

#include "FishActor.generated.h"

// A fish driven entirely by the engine-independent rules layer. The actor only maps
// 2D swim-plane state to world transforms and bone rotations.
UCLASS()
class AQUARIUM_API AFishActor : public AActor
{
	GENERATED_BODY()

public:
	AFishActor();

	UPROPERTY(EditAnywhere, Category = "Swim") uint32 Seed = 1;
	UPROPERTY(EditAnywhere, Category = "Swim") FVector PlaneOrigin = FVector(600.f, 0.f, 120.f);
	UPROPERTY(EditAnywhere, Category = "Swim") float PlaneHalfWidth = 300.f;   // cm
	UPROPERTY(EditAnywhere, Category = "Swim") float PlaneHalfHeight = 150.f;  // cm
	UPROPERTY(EditAnywhere, Category = "Swim") float AvoidDistance = 50.f;
	UPROPERTY(EditAnywhere, Category = "Swim") float MaxSpeed = 40.f;          // cm/s
	UPROPERTY(EditAnywhere, Category = "Swim") float Accel = 30.f;
	UPROPERTY(EditAnywhere, Category = "Swim") float Decel = 40.f;
	UPROPERTY(EditAnywhere, Category = "Swim") TObjectPtr<USkeletalMesh> FishMesh = nullptr;

	// Resets 2D state from the properties above and places the actor at the plane origin.
	void InitializeSwim();
	// Advances wander -> boundary avoidance -> motion, then applies transform and body wave.
	void StepSwim(float DeltaSeconds);
	float CurrentSpeed() const { return Motion.velocity.Length(); }

	void SetMesh(USkeletalMesh* Mesh);
	bool HasBone(FName Bone) const;
	// Non-const: UPoseableMeshComponent bone getters are non-const in UE 5.8.
	FRotator BoneRotation(FName Bone);
	FVector BoneLocation(FName Bone);     // component space
	FTransform BoneTransform(FName Bone); // component space

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Body = nullptr;

	aquarium::SwimPlane Plane;
	aquarium::Rect Area;
	aquarium::MotionState Motion;
	aquarium::MotionParams MotionParamsValue;
	aquarium::SwimAnimParams AnimParams;
	TOptional<aquarium::WanderBehavior> Wander; // WanderBehavior has no default ctor
	float SwimPhase = 0.f;   // accumulated wave phase (rad), advanced per tick
	float LastHeadingDeg = 0.f;
	bool bHasHeading = false; // false until the first step with non-zero speed; turn rate is 0 until then

	// Chains Spine0..Tail component-space transforms from the reference pose with the wave angles.
	void ApplyBodyWave(const std::vector<float>& AnglesDeg);

	static const TArray<FName>& SpineBoneNames();
};
