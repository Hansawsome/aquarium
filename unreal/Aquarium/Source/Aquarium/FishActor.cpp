#include "FishActor.h"

#include "FishSchoolSubsystem.h"
#include "NameTagComponent.h"

#include "Engine/World.h"

#include <algorithm>

#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

namespace
{
// Shortest-arc form of Q, clamped to at most MaxAngleDeg.
FQuat LimitQuatAngle(const FQuat& InQ, float MaxAngleDeg)
{
	FQuat Q = InQ.GetNormalized();
	// A quaternion and its negation are the same rotation; the one with W >= 0 is the short way
	// round. Without this, ToAxisAndAngle can report an angle above 180 degrees and the clamp
	// below would spin the fish the long way.
	if (Q.W < 0.f)
	{
		Q = FQuat(-Q.X, -Q.Y, -Q.Z, -Q.W);
	}
	FVector Axis;
	float AngleRad = 0.f;
	Q.ToAxisAndAngle(Axis, AngleRad);
	const float MaxRad = FMath::DegreesToRadians(FMath::Max(MaxAngleDeg, 0.f));
	if (AngleRad <= MaxRad || Axis.IsNearlyZero())
	{
		return Q;
	}
	return FQuat(Axis.GetSafeNormal(), MaxRad);
}
} // namespace

AFishActor::AFishActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
	// The fish never simulates physics, but the FBX import auto-generates a physics asset whose
	// per-bone capsules are far fatter than the body (BlueTang: 43.9 x 26.0 cm against a 25.0 x 3.6 cm
	// mesh). USkinnedMeshComponent::CalcMeshBound prefers that physics AABB in editor builds, so the
	// component bounds stopped matching what is actually drawn. Fixed bounds take the skeletal mesh's
	// own bounds instead, which is both what the renderer shows and what a cooked build would use.
	Body->bComponentUseFixedSkelBounds = true;
	RootComponent = Body;
}

const TArray<FName>& AFishActor::SpineBoneNames()
{
	static const TArray<FName> Names = {TEXT("Spine0"), TEXT("Spine1"), TEXT("Spine2"),
	                                    TEXT("Spine3"), TEXT("Spine4"), TEXT("Spine5"), TEXT("Tail")};
	return Names;
}

void AFishActor::InitializeSwim()
{
	Plane.origin = {static_cast<float>(PlaneOrigin.X), static_cast<float>(PlaneOrigin.Y), static_cast<float>(PlaneOrigin.Z)};
	Plane.right = {0.f, 1.f, 0.f};
	Plane.up = {0.f, 0.f, 1.f};
	Area = {-PlaneHalfWidth, -PlaneHalfHeight, PlaneHalfWidth, PlaneHalfHeight};
	Motion = {};
	MotionParamsValue.maxSpeed = MaxSpeed;
	MotionParamsValue.accel = Accel;
	MotionParamsValue.decel = Decel;
	AnimParams.boneCount = SpineBoneNames().Num();
	FacingParamsValue.maxTurnRateDegPerSec = MaxFacingTurnRate;
	// Shallow headings keep the M3 rate exactly, so nothing about ordinary swimming changes.
	FacingParamsValue.uprightRollRateDegPerSec = MaxFacingTurnRate;
	FacingParamsValue.steepRollRateDegPerSec = SteepRollRate;
	FacingParamsValue.steepBeginSin = SteepBeginSin;
	SpeciesKeyValue = ComputeSpeciesKey();
	Wander.Emplace(Seed, Area, /*arriveRadius*/ 15.f, /*targetLifetime*/ 8.f);
	SwimPhase = 0.f;
	InputDirection = {0.f, 0.f};
	LastHeadingDeg = 0.f;
	bHasHeading = false;
	BendTurnRate = 0.f;
	if (FishMesh)
	{
		SetMesh(FishMesh);
	}
	const aquarium::Vec3 W = Plane.ToWorld(Motion.position);
	SetActorLocation(FVector(W.x, W.y, W.z));
}

void AFishActor::StepSwim(float DeltaSeconds)
{
	if (!Wander.IsSet() || DeltaSeconds <= 0.f)
	{
		return;
	}
	// Return before anything else so a paused fish freezes completely: StepMotion already honours
	// Motion.paused, but the facing slew and the body wave would otherwise keep animating in place.
	if (Motion.paused)
	{
		return;
	}

	// The wander state is advanced either way, so a fish that stops being player-controlled resumes
	// from a current target rather than a stale one.
	Wander->Update(Motion.position, DeltaSeconds);
	// Player input replaces the wander target entirely; a zero input coasts the fish to a stop.
	aquarium::Vec2 Desired = bPlayerControlled ? InputDirection : Wander->DesiredDirection(Motion.position);
	// Schooling applies to background fish only. The player's fish is never a boid: arrow keys
	// must map to motion with nothing mixed in, or the child gets "I pressed left and it went
	// somewhere else" (F-05/F-07). The other direction -- background fish reacting to the player
	// -- is handled inside AsNeighbor(), which marks the player fish avoidOnly.
	if (!bPlayerControlled && !bIsPlayerFish)
	{
		UWorld* W = GetWorld();
		UFishSchoolSubsystem* School = W ? W->GetSubsystem<UFishSchoolSubsystem>() : nullptr;
		if (School && School->bSchoolingEnabled && School->RegisteredCount() > 1)
		{
			const aquarium::Vec2 Shared{Plane.origin.y + Motion.position.x, Plane.origin.z + Motion.position.y};
			BuildSortedNeighbors(School->Neighbors(), Shared);
			if (!NeighborScratch.empty())
			{
				const aquarium::BoidsResult R = aquarium::SchoolingSteer(
					Shared, Plane.origin.x, SpeciesKeyValue, NeighborScratch.data(), NeighborScratch.size(),
					BoidsParamsValue);
				Desired = aquarium::BlendSteering(Desired, R.steer, SchoolWeight);
			}
		}
	}
	// The two boundary rules are deliberately different for the player and for background fish.
	// Player: AvoidBoundary only cancels the outward component, so pushing into a wall simply stops
	// the fish. Sliding would add motion along the wall that the child never asked for (holding Left
	// at the left edge drifted vertically in the M3 capture).
	// Background: SteerAlongBoundary keeps the speed while turning along the wall, which resolves the
	// M1/M2b follow-up where an autonomous fish stalled at a wall, let its velocity reverse through
	// zero and snapped its facing ~180 degrees. No player is watching a specific intent there.
	// The player also gets a much narrower band (PlayerAvoidDistance) so a held key rides the visible
	// edge; the wide autonomous band is what makes a wander turn read as anticipation.
	const aquarium::Vec2 Dir = bPlayerControlled
		? aquarium::AvoidBoundary(Motion.position, Desired, Area, PlayerAvoidDistance)
		: aquarium::SteerAlongBoundary(Motion.position, Desired, Area, AvoidDistance);
	aquarium::StepMotion(Motion, Dir, MotionParamsValue, DeltaSeconds);
	Motion.position = aquarium::ClampToArea(Motion.position, Area);

	const aquarium::Vec3 W = Plane.ToWorld(Motion.position);
	SetActorLocation(FVector(W.x, W.y, W.z));

	// Face the 2D velocity. The bend turn rate follows the facing we actually apply (slewed, so it is
	// bounded by MaxFacingTurnRate) rather than the raw 2D heading, which jumps ~180 deg in one step
	// when the velocity reverses at a wall and would curl the whole tail for a single frame.
	float TargetTurnRate = 0.f;
	if (CurrentSpeed() > 1e-3f)
	{
		const bool bFirstHeading = !bHasHeading;
		const float HeadingDeg = aquarium::HeadingDeg(Motion.velocity);
		// Raw 2D heading delta only provides the left/right sign of the bend.
		const float RawTurnRate = bFirstHeading ? 0.f : aquarium::TurnRateDegPerSec(LastHeadingDeg, HeadingDeg, DeltaSeconds);
		LastHeadingDeg = HeadingDeg;
		bHasHeading = true;

		// Build the facing frame from forward and world up instead of FVector::Rotation(): for a
		// YZ-plane forward, Rotation() flips yaw by 180 deg whenever the velocity crosses world Y = 0.
		// Anchoring local Z to world up keeps the dorsal fin up for every heading (anchoring the
		// lateral axis to the plane normal instead would flip local Z with the heading sign and roll
		// the fish upside-down for half of its headings). Headings within ~1.8 deg of vertical are
		// degenerate with world up, so they keep the previous up for continuity; crossing the
		// vertical then costs a 180 deg roll about the (near-vertical) forward axis, which the slew
		// below spreads over MaxFacingTurnRate. The band width bounds how far the dorsal fin can dip
		// below the horizon while inside it: sin(band) = sqrt(1 - 0.9995^2) ~= 0.032.
		const aquarium::Vec3 F = Plane.Forward(Motion.velocity);
		const FVector Fwd(F.x, F.y, F.z);
		const FQuat Target = (FMath::Abs(Fwd.Z) < 0.9995f)
			? FRotationMatrix::MakeFromXZ(Fwd, FVector::UpVector).ToQuat()
			: FRotationMatrix::MakeFromXZ(Fwd, GetActorUpVector()).ToQuat();
		// The frame is continuous, but the 2D velocity itself can reverse through zero when the fish
		// decelerates at a wall and re-accelerates the other way. Slew the facing at a bounded rate so
		// the body never snaps; the first step after InitializeSwim snaps to its initial heading.
		// Split the required rotation into a SWING (aims the nose at the new heading) and a TWIST
		// (rolls the body about the nose-tail axis) and rate-limit them separately.
		//
		// QInterpConstantTo treated both as one lump, which is what made the vertical crossing
		// cost 0.35 s: the frames on either side of straight up differ by a 180 degree twist, and
		// at 540 deg/s that is ~0.33 s of visible pirouette. The swing limit is unchanged, so
		// ordinary turning looks exactly as it did in M3; only the twist is allowed to go fast,
		// and only while the heading is steep enough that the fish is end-on to the camera.
		const FQuat Current = GetActorQuat();
		FQuat Next = Target;
		float AppliedSwingDeg = 0.f;
		if (!bFirstHeading)
		{
			const FVector CurFwd = Current.GetAxisX();
			const FVector TgtFwd = Target.GetAxisX();
			// FindBetweenNormals on exactly opposed forwards picks an arbitrary perpendicular
			// axis. That is fine here: the result is still a 180 degree swing, and the clamp
			// below spreads it over many frames at MaxFacingTurnRate.
			const FQuat SwingFull = FQuat::FindBetweenNormals(CurFwd, TgtFwd);
			const FQuat TwistFull = Target * (SwingFull * Current).Inverse();
			const FQuat Swing = LimitQuatAngle(SwingFull, aquarium::MaxSwingStepDeg(FacingParamsValue, DeltaSeconds));
			// F.z is the vertical component of the unit forward vector, i.e. sin(pitch).
			const FQuat Twist = LimitQuatAngle(TwistFull, aquarium::MaxTwistStepDeg(F.z, FacingParamsValue, DeltaSeconds));
			Next = Twist * Swing * Current;
			Next.Normalize();
			AppliedSwingDeg = FMath::RadiansToDegrees(
				FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(CurFwd, Next.GetAxisX())), -1.f, 1.f)));
		}
		SetActorRotation(Next);

		if (!bFirstHeading)
		{
			// The body bend follows the SWING only. A twist is the body rotating about its own
			// long axis; there is no reason for that to curl the tail, and feeding the lumped
			// angular distance in would have spiked the bend during the vertical crossing.
			const float AppliedTurnRate = AppliedSwingDeg / DeltaSeconds;
			TargetTurnRate = FMath::Sign(RawTurnRate) * FMath::Min(AppliedTurnRate, MaxFacingTurnRate);
		}
	}
	// Ramp the bend input at <= MaxFacingTurnRate per second so the bend itself cannot pop when the
	// slew starts at full rate (bendPerTurnRateDeg * MaxFacingTurnRate can exceed maxBendDeg).
	BendTurnRate = FMath::FInterpConstantTo(BendTurnRate, TargetTurnRate, DeltaSeconds, MaxFacingTurnRate);
	const float TurnRate = BendTurnRate;

	SwimPhase = aquarium::SwimAnimation::AdvancePhase(SwimPhase, CurrentSpeed(), DeltaSeconds, AnimParams);
	ApplyBodyWave(aquarium::SwimAnimation::BoneAngles(CurrentSpeed(), TurnRate, SwimPhase, AnimParams));
}

// Future optimization when scaling to many fish: write Body->BoneSpaceTransforms directly for
// the whole chain and call MarkRefreshTransformDirty() once, instead of one
// SetBoneTransformByName (name lookup + refresh) per bone.
void AFishActor::ApplyBodyWave(const std::vector<float>& AnglesDeg)
{
	const USkinnedAsset* Asset = Body->GetSkinnedAsset();
	if (!Asset)
	{
		return;
	}
	const FReferenceSkeleton& RefSkel = Asset->GetRefSkeleton();
	const TArray<FTransform>& RefPose = RefSkel.GetRefBonePose();
	const TArray<FName>& Bones = SpineBoneNames();

	// Start the chain from the component-space transform of Spine0's actual parent (Root).
	const int32 HeadIndex = RefSkel.FindBoneIndex(Bones[0]);
	if (HeadIndex == INDEX_NONE)
	{
		return;
	}
	FTransform ParentComp = FTransform::Identity;
	const int32 HeadParent = RefSkel.GetParentIndex(HeadIndex);
	if (HeadParent != INDEX_NONE)
	{
		ParentComp = Body->GetBoneTransformByName(RefSkel.GetBoneName(HeadParent), EBoneSpaces::ComponentSpace);
	}

	for (int32 i = 0; i < Bones.Num() && i < static_cast<int32>(AnglesDeg.size()); ++i)
	{
		const int32 BoneIndex = RefSkel.FindBoneIndex(Bones[i]);
		if (BoneIndex == INDEX_NONE)
		{
			break; // chain is broken; later bones would have the wrong parent
		}
		// The wave is a yaw about the bone's local Z (up) axis, so the swing is sideways. This axis
		// choice is rig-specific: SK_BlueTang is exported with axis_forward='X', axis_up='Z'
		// (assets/blender/make_bluetang.py), which maps each spine bone's local Z to component +Z.
		// The BodyWaveIsChained test logs the measured axes. The rotation is applied in the bone's
		// local frame (about its own head), then the reference local offset, then the parent's
		// chained transform. UE FTransform: A * B applies A first, so this is Wave -> LocalRef -> Parent.
		const FTransform Wave(FRotator(0.f, AnglesDeg[static_cast<size_t>(i)], 0.f));
		const FTransform Comp = Wave * RefPose[BoneIndex] * ParentComp;
		Body->SetBoneTransformByName(Bones[i], Comp, EBoneSpaces::ComponentSpace);
		ParentComp = Comp;
	}
}

void AFishActor::SetInputDirection(const FVector2D& Dir)
{
	const FVector2D N = Dir.GetSafeNormal();
	InputDirection = {static_cast<float>(N.X), static_cast<float>(N.Y)};
}

void AFishActor::SetPaused(bool bInPaused)
{
	Motion.paused = bInPaused;
}

void AFishActor::SetMesh(USkeletalMesh* Mesh)
{
	FishMesh = Mesh;
	Body->SetSkinnedAssetAndUpdate(Mesh);
}

bool AFishActor::HasBone(FName Bone) const
{
	return Body->GetBoneIndex(Bone) != INDEX_NONE;
}

FRotator AFishActor::BoneRotation(FName Bone)
{
	return Body->GetBoneRotationByName(Bone, EBoneSpaces::ComponentSpace);
}

FVector AFishActor::BoneLocation(FName Bone)
{
	return Body->GetBoneLocationByName(Bone, EBoneSpaces::ComponentSpace);
}

FTransform AFishActor::BoneTransform(FName Bone)
{
	return Body->GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace);
}

UNameTagComponent* AFishActor::AttachNameTag(const FText& Name)
{
	if (NameTag == nullptr)
	{
		NameTag = NewObject<UNameTagComponent>(this, TEXT("NameTag"));
		NameTag->SetupAttachment(Body);
		// The facing frame (see StepSwim) makes the actor's local up flip with the heading, so the
		// tag must not inherit Body's rotation: keep it in world space and re-anchor it each tick.
		NameTag->SetUsingAbsoluteLocation(true);
		NameTag->SetUsingAbsoluteRotation(true);
		// The player fish is normalized to a target length via SetActorScale3D (see
		// AAquariumGameMode::PlayerFishTargetLengthCm), which can scale small species up nearly 4x.
		// Without this, the tag would inherit that world scale and the Korean text would render at
		// wildly different sizes depending on which species the session assigned.
		NameTag->SetUsingAbsoluteScale(true);
		NameTag->RegisterComponent();
	}
	// The rig origin is the body center, so the bounds half-height is the distance to the top of
	// the mesh (dorsal fin included). Use the component's actual world transform (not identity) so
	// the player-fish normalization scale (see PlayerFishTargetLengthCm) is reflected here; otherwise
	// a scaled-up small species gets a tag placed well below the top of its actual on-screen body.
	const float HalfHeight = static_cast<float>(Body->CalcBounds(Body->GetComponentTransform()).BoxExtent.Z);
	// HeightMargin is a fixed world-space cm gap above the mesh, so it is added after scaling and
	// must not itself be scaled.
	NameTagHeight = HalfHeight + NameTag->HeightMargin;
	UpdateNameTagLocation();
	NameTag->SetDisplayedName(Name);
	return NameTag;
}

void AFishActor::UpdateNameTagLocation()
{
	if (NameTag)
	{
		NameTag->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, NameTagHeight));
	}
}

int32 AFishActor::ComputeSpeciesKey() const
{
	// Derived from the mesh asset rather than authored as its own property. A species id that the
	// level script would also have to write is the same duplicated-rule trap that let a prop
	// radius bug live in BOTH build_reef_m1.py and verify_scene.py until M4b, where the verifier
	// could never catch it. A fish with no mesh (test spawns) gets 0 and schools with other
	// mesh-less fish; the tests pin that explicitly rather than leaving it to chance.
	return FishMesh ? static_cast<int32>(GetTypeHash(FishMesh->GetFName())) : 0;
}

aquarium::BoidNeighbor AFishActor::AsNeighbor() const
{
	aquarium::BoidNeighbor N;
	// Shared frame: every swim plane uses right = +Y and up = +Z, so adding the plane origin back
	// gives one common 2D frame that any fish can use a direction from without conversion.
	N.position = {Plane.origin.y + Motion.position.x, Plane.origin.z + Motion.position.y};
	N.velocity = Motion.velocity;
	N.depth = Plane.origin.x;
	N.species = SpeciesKeyValue;
	// The player's fish is a neighbour to be avoided, never one to be followed. Attracting the
	// school to it would crowd exactly the fish the child is watching; ignoring it entirely would
	// let other species swim through its body.
	N.avoidOnly = bIsPlayerFish;
	return N;
}

// Copies the neighbours that can possibly matter into NeighborScratch, NEAREST FIRST.
//
// aquarium::SchoolingSteer stops accumulating alignment/cohesion once maxNeighbors mates have
// contributed, and it walks the array in order. On a registration-ordered snapshot that cap picks
// an arbitrary six fish rather than the six nearest ones, so a fish would align with school mates
// it cannot even see while ignoring the one beside it. Sorting here is the engine side's job: the
// rules layer stays a pure function of whatever list it is handed.
void AFishActor::BuildSortedNeighbors(const std::vector<aquarium::BoidNeighbor>& Snapshot,
                                      const aquarium::Vec2& Shared)
{
	NeighborScratch.clear();
	// Widest radius any neighbour could act through, so the gate never drops one that would have
	// contributed separation.
	const float MaxRadius = FMath::Max3(BoidsParamsValue.neighborRadius, BoidsParamsValue.separationRadius,
	                                    BoidsParamsValue.avoidOnlyRadius);
	const float MaxRadiusSq = MaxRadius * MaxRadius;
	for (const aquarium::BoidNeighbor& N : Snapshot)
	{
		if (FMath::Abs(N.depth - Plane.origin.x) > BoidsParamsValue.depthRadius)
		{
			continue;
		}
		const float Dx = N.position.x - Shared.x;
		const float Dy = N.position.y - Shared.y;
		const float DistSq = Dx * Dx + Dy * Dy;
		if (DistSq <= 1e-8f || DistSq > MaxRadiusSq)
		{
			continue;   // self, or too far for any rule to reach
		}
		NeighborScratch.push_back(N);
	}
	std::sort(NeighborScratch.begin(), NeighborScratch.end(),
		[&Shared](const aquarium::BoidNeighbor& A, const aquarium::BoidNeighbor& B)
		{
			const float Ax = A.position.x - Shared.x, Ay = A.position.y - Shared.y;
			const float Bx = B.position.x - Shared.x, By = B.position.y - Shared.y;
			return (Ax * Ax + Ay * Ay) < (Bx * Bx + By * By);
		});
}

void AFishActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* W = GetWorld())
	{
		if (UFishSchoolSubsystem* School = W->GetSubsystem<UFishSchoolSubsystem>())
		{
			School->Unregister(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AFishActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeSwim();
	if (UWorld* W = GetWorld())
	{
		if (UFishSchoolSubsystem* School = W->GetSubsystem<UFishSchoolSubsystem>())
		{
			School->Register(this);
		}
	}
}

void AFishActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StepSwim(DeltaSeconds);
	UpdateNameTagLocation();
}
