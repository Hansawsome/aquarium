#include "FishActor.h"

AFishActor::AFishActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
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
	Wander.Emplace(Seed, Area, /*arriveRadius*/ 15.f, /*targetLifetime*/ 8.f);
	SwimPhase = 0.f;
	LastHeadingDeg = 0.f;
	bHasHeading = false;
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

	Wander->Update(Motion.position, DeltaSeconds);
	const aquarium::Vec2 Dir = aquarium::AvoidBoundary(Motion.position, Wander->DesiredDirection(Motion.position), Area, AvoidDistance);
	aquarium::StepMotion(Motion, Dir, MotionParamsValue, DeltaSeconds);
	Motion.position = aquarium::ClampToArea(Motion.position, Area);

	const aquarium::Vec3 W = Plane.ToWorld(Motion.position);
	SetActorLocation(FVector(W.x, W.y, W.z));

	// Face the 2D velocity; turn rate comes from the 2D heading so it does not spike on yaw flips.
	float TurnRate = 0.f;
	if (CurrentSpeed() > 1e-3f)
	{
		const float HeadingDeg = aquarium::HeadingDeg(Motion.velocity);
		if (bHasHeading)
		{
			TurnRate = aquarium::TurnRateDegPerSec(LastHeadingDeg, HeadingDeg, DeltaSeconds);
		}
		LastHeadingDeg = HeadingDeg;
		bHasHeading = true;

		const aquarium::Vec3 F = Plane.Forward(Motion.velocity);
		SetActorRotation(FVector(F.x, F.y, F.z).Rotation());
	}

	SwimPhase = aquarium::SwimAnimation::AdvancePhase(SwimPhase, CurrentSpeed(), DeltaSeconds, AnimParams);
	ApplyBodyWave(aquarium::SwimAnimation::BoneAngles(CurrentSpeed(), TurnRate, SwimPhase, AnimParams));
}

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
		// Wave is a rotation about the bone's own sideways axis, applied in the bone's local frame
		// (about its own head), then the reference local offset, then the parent's chained transform.
		// UE FTransform: A * B applies A first, so this is Wave -> LocalRef -> Parent.
		const FTransform Wave(FRotator(0.f, AnglesDeg[static_cast<size_t>(i)], 0.f));
		const FTransform Comp = Wave * RefPose[BoneIndex] * ParentComp;
		Body->SetBoneTransformByName(Bones[i], Comp, EBoneSpaces::ComponentSpace);
		ParentComp = Comp;
	}
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

void AFishActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeSwim();
}

void AFishActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StepSwim(DeltaSeconds);
}
