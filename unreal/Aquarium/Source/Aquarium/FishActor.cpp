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
	LastYaw = 0.f;
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

	// Face the 2D velocity; turn rate feeds the body bend.
	const aquarium::Vec3 F = Plane.Forward(Motion.velocity);
	float TurnRate = 0.f;
	if (F.Length() > 0.f)
	{
		const FRotator Look = FVector(F.x, F.y, F.z).Rotation();
		TurnRate = FMath::FindDeltaAngleDegrees(LastYaw, Look.Yaw) / DeltaSeconds;
		LastYaw = Look.Yaw;
		SetActorRotation(Look);
	}

	SwimPhase = aquarium::SwimAnimation::AdvancePhase(SwimPhase, CurrentSpeed(), DeltaSeconds, AnimParams);
	const std::vector<float> Angles = aquarium::SwimAnimation::BoneAngles(CurrentSpeed(), TurnRate, SwimPhase, AnimParams);
	const TArray<FName>& Bones = SpineBoneNames();
	for (int32 i = 0; i < Bones.Num() && i < static_cast<int32>(Angles.size()); ++i)
	{
		if (Body->GetBoneIndex(Bones[i]) == INDEX_NONE)
		{
			continue;
		}
		// +X is the head and Z is up, so a sideways (left/right) bend is a rotation about Z: yaw.
		Body->SetBoneRotationByName(Bones[i], FRotator(0.f, Angles[static_cast<size_t>(i)], 0.f), EBoneSpaces::ComponentSpace);
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
