#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PoseableMeshComponent.h"

#include "aquarium/Boids.h"
#include "aquarium/Bounds.h"
#include "aquarium/Catch.h"
#include "aquarium/Dash.h"
#include "aquarium/Evade.h"
#include "aquarium/Facing.h"
#include "aquarium/Flee.h"
#include "aquarium/Heading.h"
#include "aquarium/Motion.h"
#include "aquarium/Obstacles.h"
#include "aquarium/Reaction.h"
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
	// How much of a background fish's desired direction comes from its school rather than its own
	// wander target. Not 1.0 on purpose: at 1.0 a whole species congeals into one block, and the
	// remaining wander is what keeps the group loose. Tune this from the clip, not from a still.
	UPROPERTY(EditAnywhere, Category = "Swim", meta = (ClampMin = "0", ClampMax = "1")) float SchoolWeight = 0.55f;
	// How far along world X a prop may be and still count as intersecting this fish's plane.
	UPROPERTY(EditAnywhere, Category = "Swim") float ObstaclePlaneHalfDepth = 40.f;
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

	// This fish as the rules layer sees it, in the shared swim frame (x = world Y, y = world Z).
	aquarium::BoidNeighbor AsNeighbor() const;
	// Equality key derived from the mesh asset. Never logged, never stored.
	int32 SpeciesKey() const { return SpeciesKeyValue; }

	// F-09..F-12: startles this fish away from a world-space touch point. The point is expected to
	// be on (or near) this fish's swim plane; only its Y and Z are used, because the plane's X is
	// what made the click hit this fish in the first place. Re-entrant by design: the rules layer
	// decides what a second touch means (ignored mid-flee, restarts during recovery).
	void ApplyFleeFrom(const FVector& WorldTouch);
	aquarium::BehaviorState FleeState() const { return Flee.State(); }
	// 이번 놀람이 어떤 모양인지. 클릭마다 달라진다(시나리오 장면 2 요구사항 2).
	aquarium::ReactionStyle StartleStyle() const { return StartleStyleValue; }
	// 놀람 중 시각 회전(도/초). 몸짓을 만들되 이동에는 영향이 없다.
	float StartleSpinDegPerSec() const;
	// 내 물고기의 재롱이 살아 있는지. 조종권과는 무관한 시각 상태다.
	bool PlayerReactionActive() const { return PlayerReactionValue.Active(); }
	// This fish as a click can see it, in the shared swim frame. Half extents are DERIVED from the
	// rendered bounds (so the player fish's 34 cm normalization scale is included automatically);
	// no per-species radius table is copied into C++.
	aquarium::ClickTarget AsClickTarget() const;

	// 돌진 한 번. **거절하지 않는다**(aquarium::DashDrive에 실패를 표현할 반환값이 없다).
	void PressDash();
	int32 DashPressCount() const { return DashPressCountValue; }
	// 공유 유영 프레임에서 본 코끝. 몸 중심이 아니라 여기가 들이받는 점이다.
	aquarium::Vec2 NosePoint() const;
	// 공유 유영 프레임에서의 속도. CurrentSpeed()는 크기만 주므로 방향이 필요하다.
	aquarium::Vec2 SwimVelocity() const { return Motion.velocity; }
	// 이 물고기를 들이받히는 쪽으로 본 것. AsClickTarget과 같은 렌더 바운드에서 파생한다.
	aquarium::RamTarget AsRamTarget() const;
	// 다가오는 추격자를 눈치챘다. 화면 좌표는 호출자(UCatchSubsystem)가 계산해 넘긴다.
	void NoticeApproach(const aquarium::Vec2& ApproachDirShared);
	int32 EvadeNoticeCount() const { return EvadeValue.NoticeCount(); }
	// 도장이 박혔는지. 세션 리셋으로만 풀린다.
	bool IsStamped() const { return bStamped; }
	// 이름표 컴포넌트를 **실제로 달고 있는지**. IsStamped()와 다르다 -- 내 물고기는
	// 도장 없이 이름표만 달고, 그 둘을 한 상태 변수로 보면 "입장 직후 모든 물고기에
	// 이름표"를 보는 시험이 아무것도 못 본다.
	bool HasNameTag() const { return NameTag != nullptr; }
	// 잡힌 물고기에 아이의 표시를 박는다. **떼는 함수는 없다** -- 실수로 잃는 것은
	// 벌이 되기 때문이다. 되돌리는 길은 나가기(ClearStampForSessionReset) 하나뿐이다.
	UNameTagComponent* ApplyStamp(const FText& ChildName);
	// 세션 리셋 전용. 이름이 길고 못생긴 것은 의도다 -- 게임 코드에서 부르면 안 된다.
	void ClearStampForSessionReset();

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
	aquarium::BoidsParams BoidsParamsValue;
	aquarium::FleeStateMachine Flee;
	aquarium::FleeParams FleeParamsValue;
	aquarium::DashDrive Dash;
	aquarium::DashParams DashParamsValue;
	aquarium::EvadeBehavior EvadeValue;
	aquarium::EvadeParams EvadeParamsValue;
	aquarium::CatchParams CatchParamsValue;
	int32 DashPressCountValue = 0;
	bool bStamped = false;
	aquarium::ReactionStyle StartleStyleValue = aquarium::ReactionStyle::Dart;
	aquarium::StartleShape StartleShapeValue;
	float StartleSpinDeg = 0.f;     // 누적 시각 회전(도)
	// 놀람의 덧붙인 회전을 **뺀** 자세. 회전 속도 제한이 자기가 만든 연출 회전을
	// 되먹임해 매 프레임 되돌리려 드는 것을 막는다 -- 그렇게 두면 덧붙인 각도만큼
	// 프레임 변화가 커져 F-13의 연속성 검사가 178도에서 터진다(실제로 터졌다).
	FQuat FacingQuat = FQuat::Identity;
	bool bHasFacingQuat = false;
	aquarium::PlayerReaction PlayerReactionValue;
	aquarium::PlayerReactionParams PlayerReactionParamsValue;
	int32 SpeciesKeyValue = 0;
	int32 ComputeSpeciesKey() const;
	void BuildSortedNeighbors(const std::vector<aquarium::BoidNeighbor>& Snapshot, const aquarium::Vec2& Shared);
	// Per-fish, distance-sorted view of the shared snapshot. aquarium::BoidsParams::maxNeighbors
	// truncates by ARRAY ORDER, not by distance, so handing the raw registration-order snapshot
	// straight in would pick an arbitrary six fish. Kept as a member so the per-tick cost is a
	// sort of a handful of already-gated neighbours and no allocation.
	std::vector<aquarium::BoidNeighbor> NeighborScratch;
	// Built ONCE in InitializeSwim. Every fish's plane X is fixed for its whole life, so the
	// "which props reach my plane" question has a constant answer; only a handful of discs
	// survive, which is why per-tick obstacle cost is a few circle tests and never 22.
	std::vector<aquarium::Obstacle> PlaneObstacles;
	aquarium::ObstacleParams ObstacleParamsValue;
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
