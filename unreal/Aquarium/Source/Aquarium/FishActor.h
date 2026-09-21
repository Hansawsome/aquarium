#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"

#include "aquarium/Bounds.h"
#include "aquarium/Facing.h"
#include "aquarium/Heading.h"
#include "aquarium/Motion.h"
#include "aquarium/SwimAnimation.h"
#include "aquarium/SwimPlane.h"
#include "aquarium/Wander.h"

#include "FishActor.generated.h"

class UNameTagComponent;

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
	// Boundary band for the player-controlled fish, deliberately much narrower than AvoidDistance:
	// the wide autonomous band makes a wander turn read as anticipation, but on a ~130 cm half-width
	// plane it would stop a held arrow key 38% of the way in, which feels like an invisible wall.
	UPROPERTY(EditAnywhere, Category = "Swim") float PlayerAvoidDistance = 10.f;
	UPROPERTY(EditAnywhere, Category = "Swim") float MaxSpeed = 40.f;          // cm/s
	UPROPERTY(EditAnywhere, Category = "Swim") float Accel = 30.f;
	UPROPERTY(EditAnywhere, Category = "Swim") float Decel = 40.f;
	// Max rate the visible facing slews toward the velocity direction (deg/s). Bounds the per-frame
	// rotation even when the 2D velocity reverses through zero at a wall. Also the max ramp rate
	// (deg/s^2) of the turn rate fed to the body bend. Note: FMath::QInterpConstantTo caps a single
	// step at 1 rad regardless of DeltaSeconds, so on hitch frames the effective cap is ~57 deg.
	UPROPERTY(EditAnywhere, Category = "Swim", meta = (ClampMin = "1")) float MaxFacingTurnRate = 540.f;
	// Twist (roll about the nose-tail axis) is rate-limited SEPARATELY from the swing that aims
	// the nose. A plane-bound fish that keeps its dorsal fin up has a facing frame that is
	// discontinuous at the two vertical headings -- crossing straight up costs a 180 degree twist,
	// which at MaxFacingTurnRate took 0.35 s and read as the pirouette recorded since M2. The
	// twist cannot be removed (it is topology), so it is spent fast while the fish is steep and
	// therefore end-on to the fixed camera, where it cannot be read as a roll.
	// See aquarium::FacingParams and docs/superpowers/specs/2026-09-21-m4c-schooling-design.md.
	UPROPERTY(EditAnywhere, Category = "Swim", meta = (ClampMin = "1")) float SteepRollRate = 2880.f;
	UPROPERTY(EditAnywhere, Category = "Swim", meta = (ClampMin = "0", ClampMax = "0.999")) float SteepBeginSin = 0.70f;
	UPROPERTY(EditAnywhere, Category = "Swim") TObjectPtr<USkeletalMesh> FishMesh = nullptr;
	// True for the fish spawned by the game mode for the active player session (F-14).
	UPROPERTY(VisibleAnywhere, Category = "Swim") bool bIsPlayerFish = false;
	// When true the fish follows SetInputDirection instead of its wander behavior (F-05).
	UPROPERTY(VisibleAnywhere, Category = "Swim") bool bPlayerControlled = false;

	// Resets 2D state from the properties above and places the actor at the plane origin.
	void InitializeSwim();
	// Advances wander -> boundary avoidance -> motion, then applies transform and body wave.
	void StepSwim(float DeltaSeconds);
	float CurrentSpeed() const { return Motion.velocity.Length(); }

	// Sets the desired swim direction in swim-plane coordinates (X = screen right, Y = screen up).
	// Stored normalized, so a longer input vector can never exceed the normal swim speed. Only read
	// when bPlayerControlled is true; a zero vector means "no input", and the fish coasts to a stop.
	void SetInputDirection(const FVector2D& Dir);
	// Freezes the whole simulation for this fish: position, facing and body wave (F-06).
	void SetPaused(bool bInPaused);
	bool IsPaused() const { return Motion.paused; }

	void SetMesh(USkeletalMesh* Mesh);
	bool HasBone(FName Bone) const;
	// Non-const: UPoseableMeshComponent bone getters are non-const in UE 5.8.
	FRotator BoneRotation(FName Bone);
	// Test/diagnostic helpers (component space); not used by gameplay.
	FVector BoneLocation(FName Bone);
	FTransform BoneTransform(FName Bone);

	// Attaches a screen-space name tag above the body (F-04). Intended for the player fish only.
	UNameTagComponent* AttachNameTag(const FText& Name);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Body = nullptr;
	UPROPERTY() TObjectPtr<UNameTagComponent> NameTag = nullptr;
	// World-space height of the name tag anchor above the actor origin (bounds + margin).
	float NameTagHeight = 0.f;
	void UpdateNameTagLocation();

	aquarium::SwimPlane Plane;
	aquarium::Rect Area;
	aquarium::MotionState Motion;
	aquarium::MotionParams MotionParamsValue;
	aquarium::SwimAnimParams AnimParams;
	aquarium::FacingParams FacingParamsValue;
	TOptional<aquarium::WanderBehavior> Wander; // WanderBehavior has no default ctor
	float SwimPhase = 0.f;   // accumulated wave phase (rad), advanced per tick
	float LastHeadingDeg = 0.f;
	bool bHasHeading = false; // false until the first step with non-zero speed; turn rate is 0 until then
	aquarium::Vec2 InputDirection{0.f, 0.f}; // normalized; only used when bPlayerControlled
	float BendTurnRate = 0.f; // signed deg/s fed to the body bend; ramp-limited, follows the slewed facing

	// Chains Spine0..Tail component-space transforms from the reference pose with the wave angles.
	void ApplyBodyWave(const std::vector<float>& AnglesDeg);

	static const TArray<FName>& SpineBoneNames();
};
