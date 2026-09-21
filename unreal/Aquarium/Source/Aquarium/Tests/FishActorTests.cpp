#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "FishActor.h"
#include "FishSchoolSubsystem.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "aquarium/Boids.h"
#include "aquarium/Facing.h"
#include "aquarium/Flee.h"

namespace
{
AFishActor* SpawnFish(UWorld* World, uint32 Seed)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFishActor* Fish = World->SpawnActor<AFishActor>(AFishActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	Fish->Seed = Seed;
	Fish->PlaneOrigin = FVector(300.f, 0.f, 100.f);
	Fish->PlaneHalfWidth = 300.f;
	Fish->PlaneHalfHeight = 150.f;
	Fish->InitializeSwim();
	return Fish;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorMovesByRules, "Aquarium.Fish.MovesByRulesLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorMovesByRules::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	const FVector Before = Fish->GetActorLocation();
	for (int i = 0; i < 30; ++i) Fish->StepSwim(0.1f);
	const FVector After = Fish->GetActorLocation();
	TestTrue(TEXT("fish moved"), !After.Equals(Before, 1.f));
	TestTrue(TEXT("stays on plane X"), FMath::IsNearlyEqual(After.X, 300.f, 1e-2f));
	TestTrue(TEXT("inside plane Y"), FMath::Abs(After.Y) <= 300.f + 1e-2f);
	TestTrue(TEXT("inside plane Z"), FMath::Abs(After.Z - 100.f) <= 150.f + 1e-2f);
	TestTrue(TEXT("speed reported"), Fish->CurrentSpeed() > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorZeroDt, "Aquarium.Fish.ZeroDtDoesNotMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorZeroDt::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->StepSwim(0.1f);
	const FVector Before = Fish->GetActorLocation();
	Fish->StepSwim(0.f);
	TestTrue(TEXT("no move at dt=0"), Fish->GetActorLocation().Equals(Before, 1e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorDeterministic, "Aquarium.Fish.SameSeedSamePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorDeterministic::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* A = SpawnFish(World, 42u);
	AFishActor* B = SpawnFish(World, 42u);
	for (int i = 0; i < 50; ++i) { A->StepSwim(0.05f); B->StepSwim(0.05f); }
	TestTrue(TEXT("same path"), A->GetActorLocation().Equals(B->GetActorLocation(), 1e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorSpineBonesExist, "Aquarium.Fish.SpineBonesExistOnMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorSpineBonesExist::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 1u);
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Mesh)) return false;
	Fish->SetMesh(Mesh);
	for (const TCHAR* Name : {TEXT("Spine0"), TEXT("Spine5"), TEXT("Tail")})
		TestTrue(FString::Printf(TEXT("bone %s exists"), Name), Fish->HasBone(FName(Name)));
	Fish->StepSwim(0.1f);
	TestTrue(TEXT("tail bone rotated by swim wave"),
		!Fish->BoneRotation(FName(TEXT("Tail"))).IsNearlyZero(1e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorBodyWaveIsChained, "Aquarium.Fish.BodyWaveIsChained",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorBodyWaveIsChained::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Mesh)) return false;
	Fish->SetMesh(Mesh);

	// Log the reference local axes of a spine bone once, to document which axis bends sideways.
	const FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
	const int32 Spine1Index = RefSkel.FindBoneIndex(FName(TEXT("Spine1")));
	if (TestTrue(TEXT("Spine1 in ref skeleton"), Spine1Index != INDEX_NONE))
	{
		const FQuat Q = RefSkel.GetRefBonePose()[Spine1Index].GetRotation();
		UE_LOG(LogTemp, Verbose, TEXT("Spine1 local ref axes: X=%s Y=%s Z=%s T=%s"),
			*Q.GetAxisX().ToString(), *Q.GetAxisY().ToString(), *Q.GetAxisZ().ToString(),
			*RefSkel.GetRefBonePose()[Spine1Index].GetTranslation().ToString());
	}

	const FQuat Spine1Comp = Fish->BoneTransform(FName(TEXT("Spine1"))).GetRotation();
	UE_LOG(LogTemp, Verbose, TEXT("Spine1 component-space axes: X=%s Y=%s Z=%s"),
		*Spine1Comp.GetAxisX().ToString(), *Spine1Comp.GetAxisY().ToString(), *Spine1Comp.GetAxisZ().ToString());

	const FVector TailRef = Fish->BoneLocation(FName(TEXT("Tail")));
	const FVector Spine0Ref = Fish->BoneLocation(FName(TEXT("Spine0")));
	for (int i = 0; i < 5; ++i) Fish->StepSwim(0.1f);
	const FVector TailNow = Fish->BoneLocation(FName(TEXT("Tail")));
	const FVector Spine0Now = Fish->BoneLocation(FName(TEXT("Spine0")));
	const FVector TailDelta = TailNow - TailRef;
	UE_LOG(LogTemp, Verbose, TEXT("Tail ref=%s now=%s delta=%s"), *TailRef.ToString(), *TailNow.ToString(), *TailDelta.ToString());
	TestTrue(TEXT("tail location moved by chained parent rotations"), TailDelta.Size() > 0.1f);
	TestTrue(TEXT("tail swings sideways (component Y), not up/down"), FMath::Abs(TailDelta.Y) > FMath::Abs(TailDelta.Z));
	TestTrue(TEXT("Spine0 location unchanged (rotates about its own head)"), Spine0Now.Equals(Spine0Ref, 1e-3f));
	TestTrue(TEXT("tail bone rotated by swim wave"),
		!Fish->BoneRotation(FName(TEXT("Tail"))).IsNearlyZero(1e-3f));

	// Chaining invariant: a child's offset from its parent is the reference local translation.
	const int32 TailIndex = RefSkel.FindBoneIndex(FName(TEXT("Tail")));
	if (TestTrue(TEXT("Tail in ref skeleton"), TailIndex != INDEX_NONE))
	{
		const FTransform TailRelSpine5 = Fish->BoneTransform(FName(TEXT("Tail"))).GetRelativeTransform(Fish->BoneTransform(FName(TEXT("Spine5"))));
		TestTrue(TEXT("Tail keeps its reference offset from Spine5"),
			TailRelSpine5.GetTranslation().Equals(RefSkel.GetRefBonePose()[TailIndex].GetTranslation(), 1e-3f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFacingIsContinuous, "Aquarium.Fish.FacingIsContinuous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFacingIsContinuous::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->PlaneHalfWidth = 60.f; // narrow: AvoidBoundary fires often, velocity crosses vertical routinely
	Fish->PlaneHalfHeight = 150.f;
	Fish->InitializeSwim();

	// The old bound was a single lumped quaternion distance, which could not distinguish the nose
	// sweeping from the body twisting. The twist across a vertical heading now deliberately
	// exceeds that lump, so the bound is split into the two things it was really standing in for.
	// Both limits are DERIVED from the rules-layer functions rather than re-typed here: a bound
	// copied into the test is a bound that can silently disagree with the code it guards.
	constexpr float Dt = 0.05f;
	aquarium::FacingParams FP;
	FP.maxTurnRateDegPerSec = Fish->MaxFacingTurnRate;
	FP.uprightRollRateDegPerSec = Fish->MaxFacingTurnRate;
	FP.steepRollRateDegPerSec = Fish->SteepRollRate;
	FP.steepBeginSin = Fish->SteepBeginSin;
	const float MaxSwingDeg = aquarium::MaxSwingStepDeg(FP, Dt) + 1.f;

	Fish->StepSwim(Dt);
	FQuat PrevQuat = Fish->GetActorQuat();
	for (int i = 1; i < 300; ++i)
	{
		Fish->StepSwim(Dt);
		const FQuat Now = Fish->GetActorQuat();
		// (1) the nose may never sweep faster than MaxFacingTurnRate.
		const float SwingDeg = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(PrevQuat.GetAxisX(), Now.GetAxisX())), -1.f, 1.f)));
		if (!TestTrue(FString::Printf(TEXT("step %d: nose swung %.1f deg (limit %.1f)"), i, SwingDeg, MaxSwingDeg), SwingDeg < MaxSwingDeg))
		{
			return false;
		}
		// (2) the twist may never exceed what this step's steepness permits. The steepness is
		// taken from the heading the fish actually ended the step on.
		const float VerticalSin = static_cast<float>(Now.GetAxisX().Z);
		const float MaxTwistDeg = aquarium::MaxTwistStepDeg(VerticalSin, FP, Dt) + 1.f;
		const FQuat Swing = FQuat::FindBetweenNormals(PrevQuat.GetAxisX(), Now.GetAxisX());
		FQuat Twist = Now * (Swing * PrevQuat).Inverse();
		Twist.Normalize();
		if (Twist.W < 0.f) Twist = FQuat(-Twist.X, -Twist.Y, -Twist.Z, -Twist.W);
		FVector TwistAxis;
		float TwistRad = 0.f;
		Twist.ToAxisAndAngle(TwistAxis, TwistRad);
		const float TwistDeg = FMath::RadiansToDegrees(TwistRad);
		if (!TestTrue(FString::Printf(TEXT("step %d: body twisted %.1f deg (limit %.1f at sin %.3f)"), i, TwistDeg, MaxTwistDeg, VerticalSin), TwistDeg < MaxTwistDeg))
		{
			return false;
		}
		PrevQuat = Now;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorBoneAnglesAreContinuous, "Aquarium.Fish.BoneAnglesAreContinuous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorBoneAnglesAreContinuous::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->PlaneHalfWidth = 60.f; // narrow: wall reversals are routine
	Fish->PlaneHalfHeight = 150.f;
	Fish->InitializeSwim();
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Mesh)) return false;
	Fish->SetMesh(Mesh);

	// Bound on the per-step change of Tail's component-space rotation. Chaining sums the yaw of all
	// spine bones, so both the wave and the bend terms are summed over the chain gains.
	constexpr float Dt = 0.05f;
	aquarium::SwimAnimParams P;
	P.boneCount = 7; // Spine0..Spine5 + Tail, as in AFishActor::SpineBoneNames
	const float SumGain = P.boneCount + P.tailGain * P.boneCount * (P.boneCount - 1) / 2.f;
	const float Amp = aquarium::SwimAnimation::Amplitude(Fish->MaxSpeed, P);
	const float Freq = aquarium::SwimAnimation::Frequency(Fish->MaxSpeed, P);
	const float WavePhaseStep = Amp * SumGain * 2.f * PI * Freq * Dt;            // phase advance at max speed
	const float WaveAmpStep = P.amplitudePerSpeedDeg * Fish->Accel * Dt * SumGain; // amplitude change with speed
	const float BendStep = P.bendPerTurnRateDeg * Fish->MaxFacingTurnRate * Dt * P.boneCount; // turn rate ramps at <= MaxFacingTurnRate/s
	const float MaxTailStepDeg = WavePhaseStep + WaveAmpStep + BendStep + 1.f;

	Fish->StepSwim(Dt); // warm-up: first step snaps to the initial heading
	FQuat Prev = Fish->BoneTransform(FName(TEXT("Tail"))).GetRotation();
	for (int i = 1; i < 300; ++i)
	{
		Fish->StepSwim(Dt);
		const FQuat Now = Fish->BoneTransform(FName(TEXT("Tail"))).GetRotation();
		const float StepDeg = FMath::RadiansToDegrees(Prev.AngularDistance(Now));
		if (!TestTrue(FString::Printf(TEXT("step %d: tail rotation jumped %.1f deg (bound %.1f)"), i, StepDeg, MaxTailStepDeg), StepDeg < MaxTailStepDeg))
		{
			return false;
		}
		Prev = Now;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorUpVectorStaysUpright, "Aquarium.Fish.UpVectorStaysUpright",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorUpVectorStaysUpright::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->PlaneHalfWidth = 60.f; // narrow: the heading sweeps through both +Y and -Y often
	Fish->PlaneHalfHeight = 150.f;
	Fish->InitializeSwim();

	// The fish swims on a vertical plane; its dorsal fin (local +Z) must stay above the horizon,
	// whichever way it heads. At an exactly vertical heading the up vector is horizontal (up.Z == 0)
	// and StepSwim holds the previous up while the heading is within ~1.8 deg of vertical, so allow
	// a dip of a few degrees there; the bug this guards against rolls up.Z to about -1.
	constexpr float MinUpZTolerance = -0.05f;
	constexpr float Dt = 0.05f;
	Fish->StepSwim(Dt); // warm-up: first step snaps to the initial heading
	float MinUpZ = 1.f;
	int MinStep = 0;
	FVector MinUp = FVector::UpVector;
	for (int i = 1; i < 300; ++i)
	{
		Fish->StepSwim(Dt);
		const FVector Up = Fish->GetActorUpVector();
		if (Up.Z < MinUpZ)
		{
			MinUpZ = Up.Z;
			MinStep = i;
			MinUp = Up;
		}
	}
	return TestTrue(FString::Printf(TEXT("lowest up vector at step %d points down (up = %s, up.Z = %.3f, tolerance %.3f)"), MinStep, *MinUp.ToString(), MinUpZ, MinUpZTolerance), MinUpZ > MinUpZTolerance);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFollowsInput, "Aquarium.Fish.PlayerControlledFollowsInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFollowsInput::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->bPlayerControlled = true;
	Fish->SetInputDirection(FVector2D(1.f, 0.f));          // screen right
	const FVector Before = Fish->GetActorLocation();
	for (int i = 0; i < 20; ++i) Fish->StepSwim(0.05f);
	const FVector After = Fish->GetActorLocation();
	TestTrue(TEXT("moved along +Y (screen right)"), After.Y - Before.Y > 5.f);
	TestTrue(TEXT("did not drift vertically"), FMath::Abs(After.Z - Before.Z) < 2.f);
	Fish->SetInputDirection(FVector2D::ZeroVector);
	for (int i = 0; i < 40; ++i) Fish->StepSwim(0.05f);
	TestTrue(TEXT("stopped after release"), Fish->CurrentSpeed() < 1.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorPauseFreezes, "Aquarium.Fish.PausedDoesNotMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorPauseFreezes::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->bPlayerControlled = true;
	Fish->SetInputDirection(FVector2D(1.f, 0.f));
	for (int i = 0; i < 10; ++i) Fish->StepSwim(0.05f);
	Fish->SetPaused(true);
	const FVector Frozen = Fish->GetActorLocation();
	for (int i = 0; i < 20; ++i) Fish->StepSwim(0.05f);
	TestTrue(TEXT("no movement while paused"), Fish->GetActorLocation().Equals(Frozen, 1e-3f));
	Fish->SetPaused(false);
	Fish->StepSwim(5.0f);
	TestTrue(TEXT("no teleport on resume"), FVector::Dist(Fish->GetActorLocation(), Frozen) < 60.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorBackgroundIgnoresInput, "Aquarium.Fish.BackgroundIgnoresInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorBackgroundIgnoresInput::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* A = SpawnFish(World, 42u);
	AFishActor* B = SpawnFish(World, 42u);
	B->SetInputDirection(FVector2D(1.f, 0.f));             // not player-controlled: ignored
	for (int i = 0; i < 40; ++i) { A->StepSwim(0.05f); B->StepSwim(0.05f); }
	TestTrue(TEXT("same wander path"), A->GetActorLocation().Equals(B->GetActorLocation(), 1e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorPlayerStopsAtWall, "Aquarium.Fish.PlayerStopsAtWallWithoutDrift", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorPlayerStopsAtWall::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 7u);
	Fish->bPlayerControlled = true;
	// Swim up and to the right first so the fish sits off-centre vertically when it meets the wall:
	// that is the M3 capture case, where the boundary steering has a centre to aim back at.
	Fish->SetInputDirection(FVector2D(0.7071f, 0.7071f));
	for (int i = 0; i < 80; ++i) Fish->StepSwim(0.05f);
	// Now hold "screen right" alone, long enough to reach and settle against the right wall.
	Fish->SetInputDirection(FVector2D(1.f, 0.f));
	for (int i = 0; i < 120; ++i) Fish->StepSwim(0.05f);
	const FVector Settled = Fish->GetActorLocation();
	// Keep holding: the fish must not acquire vertical motion it was never asked for.
	float MaxDrift = 0.f;
	for (int i = 0; i < 100; ++i)
	{
		Fish->StepSwim(0.05f);
		MaxDrift = FMath::Max(MaxDrift, FMath::Abs(Fish->GetActorLocation().Z - Settled.Z));
	}
	const FVector After = Fish->GetActorLocation();
	TestTrue(FString::Printf(TEXT("no unforced vertical drift at the wall (max %.2f cm)"), MaxDrift), MaxDrift < 2.f);
	TestTrue(TEXT("stays inside the plane"), FMath::Abs(After.Y - Fish->PlaneOrigin.Y) <= Fish->PlaneHalfWidth + 1.f);
	// ...and it really reaches the edge: stopping a whole AvoidDistance short would read as an
	// invisible wall well inside the visible plane.
	const float DistanceFromEdge = Fish->PlaneHalfWidth - static_cast<float>(After.Y - Fish->PlaneOrigin.Y);
	TestTrue(FString::Printf(TEXT("rides the plane edge (%.2f cm from it)"), DistanceFromEdge),
		DistanceFromEdge <= Fish->PlayerAvoidDistance + 1.f);
	return true;
}

// Diagnoses (and, after Task 5, guards) the vertical-transition pirouette recorded since M2.
//
// The fish is driven by hand through a heading sweep that crosses straight up: up-and-right,
// then straight up, then up-and-left. The facing frame is MakeFromXZ(Fwd, worldUp), whose local
// Z flips sign the moment the lateral component of an almost vertical heading changes sign, so
// the two frames differ by a 180 degree twist about the (almost vertical) forward axis.
//
// Twist is measured as the rotation about the forward axis, separated from the swing that aims
// the nose -- the same decomposition StepSwim uses.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorNoLongTwistAcrossVertical, "Aquarium.Fish.FacingHasNoLongTwistAcrossVertical",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorNoLongTwistAcrossVertical::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = SpawnFish(World, 11u);
	Fish->bPlayerControlled = true;   // drive the heading by hand, no wander in the way
	Fish->PlaneHalfWidth = 400.f;
	Fish->PlaneHalfHeight = 400.f;
	Fish->InitializeSwim();

	constexpr float Dt = 1.f / 60.f;
	// Hold each direction long enough for the velocity to actually reach it.
	const TArray<FVector2D> Legs = {FVector2D(0.35f, 1.f), FVector2D(0.f, 1.f), FVector2D(-0.35f, 1.f)};
	float TotalTwistDeg = 0.f;
	int TwistingSteps = 0;
	float PeakTwistRate = 0.f;

	Fish->SetInputDirection(Legs[0]);
	for (int i = 0; i < 90; ++i) Fish->StepSwim(Dt);   // settle onto the first heading

	FQuat Prev = Fish->GetActorQuat();
	for (int Leg = 1; Leg < Legs.Num(); ++Leg)
	{
		Fish->SetInputDirection(Legs[Leg]);
		for (int i = 0; i < 120; ++i)
		{
			Fish->StepSwim(Dt);
			const FQuat Now = Fish->GetActorQuat();
			// Swing takes Prev's forward to Now's forward; whatever is left is twist.
			const FQuat Swing = FQuat::FindBetweenNormals(Prev.GetAxisX(), Now.GetAxisX());
			FQuat Twist = Now * (Swing * Prev).Inverse();
			Twist.Normalize();
			if (Twist.W < 0.f) Twist = FQuat(-Twist.X, -Twist.Y, -Twist.Z, -Twist.W);
			FVector Axis; float AngleRad;
			Twist.ToAxisAndAngle(Axis, AngleRad);
			const float StepTwistDeg = FMath::RadiansToDegrees(AngleRad);
			if (StepTwistDeg > 0.5f)
			{
				TotalTwistDeg += StepTwistDeg;
				++TwistingSteps;
				PeakTwistRate = FMath::Max(PeakTwistRate, StepTwistDeg / Dt);
			}
			Prev = Now;
		}
	}

	// The flip itself is unavoidable, so this does NOT assert that no twist happens. It asserts
	// that the twist never drags on: at the steep rate 180 degrees fits in 0.07 s, which is 5
	// steps at 60 fps. The defect took 0.33 s, i.e. 20 steps.
	const float ElapsedSec = static_cast<float>(TwistingSteps) * Dt;
	AddInfo(FString::Printf(TEXT("twist total %.1f deg over %d steps (%.3f s), peak %.0f deg/s"),
		TotalTwistDeg, TwistingSteps, ElapsedSec, PeakTwistRate));
	return TestTrue(FString::Printf(TEXT("twist across vertical took %.3f s (limit 0.12 s), total %.1f deg"), ElapsedSec, TotalTwistDeg),
		ElapsedSec < 0.12f);
}

namespace
{
// Registers a fish with the world's school subsystem and parks it at a fixed plane position.
AFishActor* SchoolFish(UWorld* World, uint32 Seed, const FVector& Origin, USkeletalMesh* Mesh)
{
	AFishActor* Fish = SpawnFish(World, Seed);
	Fish->PlaneOrigin = Origin;
	Fish->PlaneHalfWidth = 400.f;
	Fish->PlaneHalfHeight = 400.f;
	Fish->FishMesh = Mesh;
	Fish->InitializeSwim();
	World->GetSubsystem<UFishSchoolSubsystem>()->Register(Fish);
	return Fish;
}
} // namespace

namespace
{
// Runs three same-species fish, 120 cm apart, for 20 simulated seconds at the given school weight
// and returns the MEAN spread of the outer pair over the second half.
//
// Mean, not final: one final sample lands wherever the wander cycle happens to be. Second half,
// not the whole run: the group starts 240 cm apart by construction, and no steering can compress
// that instantly, so including the settling phase buries the difference being measured (a peak
// over the whole run reads 240 for both weights -- measured).
float SteadyOuterSpread(UWorld* World, USkeletalMesh* Mesh, float Weight)
{
	AFishActor* A = SchoolFish(World, 3u, FVector(400.f, -120.f, 150.f), Mesh);
	AFishActor* B = SchoolFish(World, 4u, FVector(400.f, 0.f, 150.f), Mesh);
	AFishActor* C = SchoolFish(World, 5u, FVector(400.f, 120.f, 150.f), Mesh);
	A->SchoolWeight = Weight;
	B->SchoolWeight = Weight;
	C->SchoolWeight = Weight;
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	double Sum = 0.0;
	int Samples = 0;
	for (int i = 0; i < 400; ++i)
	{
		School->InvalidateSnapshot();
		A->StepSwim(0.05f);
		B->StepSwim(0.05f);
		C->StepSwim(0.05f);
		if (i >= 200)
		{
			Sum += FMath::Abs(A->GetActorLocation().Y - C->GetActorLocation().Y);
			++Samples;
		}
	}
	return static_cast<float>(Sum / FMath::Max(Samples, 1));
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorSchoolMatesPullTogether, "Aquarium.Fish.SchoolMatesPullTogether",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorSchoolMatesPullTogether::RunTest(const FString&)
{
	USkeletalMesh* Tang = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Tang)) return false;

	// The same three seeds are run twice, in two fresh worlds, differing ONLY in SchoolWeight.
	//
	// The plan's version of this test asserted a single absolute limit (spread < 400 cm) against
	// the schooled run alone. That guard was vacuous: with SchoolWeight forced to 0 these seeds
	// still came in at 129 cm, so the test passed with schooling switched off entirely and could
	// never have caught a broken wiring. Comparing the two runs makes the assertion about
	// schooling rather than about the seeds.
	UWorld* Loose = FAutomationEditorCommonUtils::CreateNewMap();
	const float Unschooled = SteadyOuterSpread(Loose, Tang, 0.f);
	UWorld* Tight = FAutomationEditorCommonUtils::CreateNewMap();
	const float Schooled = SteadyOuterSpread(Tight, Tang, 0.55f);
	TestEqual(TEXT("three fish registered"), Tight->GetSubsystem<UFishSchoolSubsystem>()->RegisteredCount(), 3);

	AddInfo(FString::Printf(TEXT("steady outer spread: unschooled %.1f cm, schooled %.1f cm (ratio %.2f)"),
		Unschooled, Schooled, Schooled / FMath::Max(Unschooled, 1.f)));
	// Cohesion must visibly tighten the group, and must also hold it inside roughly one neighbour
	// radius plus the separation the school keeps between its members.
	TestTrue(FString::Printf(TEXT("schooling tightened the group (%.1f vs %.1f cm)"), Schooled, Unschooled),
		Schooled < Unschooled * 0.75f);
	return TestTrue(FString::Printf(TEXT("school stayed together (mean %.1f cm, limit 250)"), Schooled),
		Schooled < 250.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorOtherSpeciesDoNotPull, "Aquarium.Fish.OtherSpeciesDoNotPull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorOtherSpeciesDoNotPull::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	USkeletalMesh* Tang = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	USkeletalMesh* Clown = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/Clownfish/SK_Clownfish.SK_Clownfish"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Tang)) return false;
	if (!TestNotNull(TEXT("SK_Clownfish loads"), Clown)) return false;

	AFishActor* A = SchoolFish(World, 3u, FVector(400.f, 0.f, 150.f), Tang);
	AFishActor* B = SchoolFish(World, 4u, FVector(400.f, 100.f, 150.f), Clown);
	TestNotEqual(TEXT("species keys differ"), A->SpeciesKey(), B->SpeciesKey());

	// A blue tang 100 cm from a clownfish is inside neighborRadius but outside separationRadius,
	// so the clownfish must contribute exactly nothing to the tang's steer.
	const aquarium::BoidNeighbor N = B->AsNeighbor();
	aquarium::BoidsParams P;
	const aquarium::BoidsResult R = aquarium::SchoolingSteer(A->AsNeighbor().position, A->AsNeighbor().depth,
		A->SpeciesKey(), &N, 1, P);
	TestEqual(TEXT("no alignment/cohesion from another species"), R.consideredCount, 0);
	TestEqual(TEXT("no separation at 100 cm from another species"), R.avoidCount, 0);
	// But at 20 cm it IS separated from: a clownfish must not swim through a blue tang.
	B->PlaneOrigin = FVector(400.f, 20.f, 150.f);
	B->InitializeSwim();
	const aquarium::BoidNeighbor Close = B->AsNeighbor();
	const aquarium::BoidsResult R2 = aquarium::SchoolingSteer(A->AsNeighbor().position, A->AsNeighbor().depth,
		A->SpeciesKey(), &Close, 1, P);
	TestEqual(TEXT("separation from another species at 20 cm"), R2.avoidCount, 1);
	return TestTrue(TEXT("pushed away from the other species"), R2.steer.x < -0.9f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorPlayerFishIsAvoidedNotFollowed, "Aquarium.Fish.PlayerFishIsAvoidedNotFollowed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorPlayerFishIsAvoidedNotFollowed::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	USkeletalMesh* Tang = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Tang)) return false;

	AFishActor* Background = SchoolFish(World, 3u, FVector(400.f, 0.f, 150.f), Tang);
	AFishActor* Player = SchoolFish(World, 4u, FVector(400.f, 90.f, 150.f), Tang);  // SAME species
	Player->bIsPlayerFish = true;

	const aquarium::BoidNeighbor N = Player->AsNeighbor();
	TestTrue(TEXT("the player fish is marked avoidOnly"), N.avoidOnly);
	aquarium::BoidsParams P;
	const aquarium::BoidsResult R = aquarium::SchoolingSteer(Background->AsNeighbor().position,
		Background->AsNeighbor().depth, Background->SpeciesKey(), &N, 1, P);
	TestEqual(TEXT("never a cohesion/alignment target, even same species"), R.consideredCount, 0);
	TestEqual(TEXT("avoided at 90 cm (avoidOnlyRadius 110)"), R.avoidCount, 1);
	return TestTrue(TEXT("the school opens away from the player fish"), R.steer.x < -0.9f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorPlayerFishIgnoresSchooling, "Aquarium.Fish.PlayerFishIgnoresSchooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorPlayerFishIgnoresSchooling::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	USkeletalMesh* Tang = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Tang)) return false;

	// A crowd of same-species fish sits hard to the LEFT of the player fish. If the player fish
	// were a boid at all, cohesion would bend it left. It must go exactly where the key says.
	AFishActor* Player = SchoolFish(World, 9u, FVector(400.f, 0.f, 150.f), Tang);
	Player->bIsPlayerFish = true;
	Player->bPlayerControlled = true;
	Player->SetInputDirection(FVector2D(1.f, 0.f));   // screen right
	for (int i = 0; i < 6; ++i)
	{
		SchoolFish(World, 20u + static_cast<uint32>(i), FVector(400.f, -100.f - 15.f * i, 150.f), Tang);
	}
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	const FVector Before = Player->GetActorLocation();
	for (int i = 0; i < 40; ++i)
	{
		School->InvalidateSnapshot();
		Player->StepSwim(0.05f);
	}
	const FVector After = Player->GetActorLocation();
	TestTrue(TEXT("moved right as instructed"), After.Y - Before.Y > 5.f);
	return TestTrue(FString::Printf(TEXT("did not drift vertically (dz = %.3f)"), After.Z - Before.Z),
		FMath::Abs(After.Z - Before.Z) < 2.f);
}

namespace
{
// Spawns a tagged box prop at a world location, sized like a coral.
AStaticMeshActor* SpawnProp(UWorld* World, const FVector& Location, const FVector& Scale)
{
	AStaticMeshActor* Prop = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, FRotator::ZeroRotator);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Prop->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Prop->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Prop->SetActorScale3D(Scale);
	Prop->Tags.Add(UFishSchoolSubsystem::PropTag);
	return Prop;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorObstaclesDerivedFromPropBounds, "Aquarium.Fish.ObstaclesDerivedFromPropBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorObstaclesDerivedFromPropBounds::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	// /Engine/BasicShapes/Cube is 100 cm, so scale 1 gives half extents of 50 cm.
	SpawnProp(World, FVector(400.f, 100.f, 60.f), FVector(1.f, 1.f, 3.f));   // reaches the plane
	SpawnProp(World, FVector(900.f, 100.f, 60.f), FVector(1.f, 1.f, 1.f));   // 5 m away in depth
	// A collapsed bound is not a small obstacle: with the rule's 20 cm margin it would become an
	// invisible wall. It must produce no discs at all.
	SpawnProp(World, FVector(400.f, -150.f, 150.f), FVector(1.f, 0.f, 0.f));

	std::vector<aquarium::Obstacle> Obs;
	School->BuildObstaclesForPlane(FVector(400.f, 0.f, 150.f), 40.f, Obs);
	TestTrue(TEXT("the distant prop and the degenerate prop are filtered out"),
		Obs.size() <= static_cast<size_t>(UFishSchoolSubsystem::MaxDiscsPerProp));
	if (!TestTrue(TEXT("the near prop produced discs"), !Obs.empty())) return false;
	for (const aquarium::Obstacle& O : Obs)
	{
		TestTrue(TEXT("no ghost disc from a degenerate bound"), O.center.x > 0.f);
	}
	// Radius is the HALF WIDTH of the actual bounds (50 cm), derived, not a copied table value.
	TestTrue(FString::Printf(TEXT("radius %.1f is the actor's half width"), Obs[0].radius), FMath::IsNearlyEqual(Obs[0].radius, 50.f, 1.f));
	// A 150 cm half-height prop against a 50 cm radius asks for 3 discs.
	TestEqual(TEXT("a tall prop is a stack, not one fat disc"), static_cast<int32>(Obs.size()), 3);
	// Plane-local: the prop is at world Y = 100 and the plane origin at Y = 0.
	return TestTrue(FString::Printf(TEXT("disc centre x %.1f is plane-local"), Obs[0].center.x), FMath::IsNearlyEqual(Obs[0].center.x, 100.f, 1.f));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorSwimsAroundProp, "Aquarium.Fish.SwimsAroundPropInsteadOfThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorSwimsAroundProp::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	SpawnProp(World, FVector(400.f, 0.f, 150.f), FVector(1.f, 1.f, 1.f));   // dead ahead, r = 50

	AFishActor* Fish = SpawnFish(World, 5u);
	Fish->bPlayerControlled = true;
	Fish->PlaneOrigin = FVector(400.f, -250.f, 150.f);
	Fish->PlaneHalfWidth = 400.f;
	Fish->PlaneHalfHeight = 200.f;
	Fish->InitializeSwim();
	Fish->SetInputDirection(FVector2D(1.f, 0.f));   // straight at the prop

	float MinDist = 1e9f;
	for (int i = 0; i < 400; ++i)
	{
		Fish->StepSwim(0.05f);
		const FVector L = Fish->GetActorLocation();
		MinDist = FMath::Min(MinDist, static_cast<float>(FVector2D(L.Y - 0.f, L.Z - 150.f).Size()));
	}
	AddInfo(FString::Printf(TEXT("closest approach %.1f cm to a 50 cm prop"), MinDist));
	// Clearance, not contact: the fish must turn BEFORE it arrives. 50 cm radius, and the rule
	// adds a 20 cm margin, so anything under 50 means it went through the solid part.
	return TestTrue(FString::Printf(TEXT("kept clear of the prop (closest %.1f cm, limit 50)"), MinDist), MinDist > 50.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorSpeedSurvivesPropAvoidance, "Aquarium.Fish.SpeedSurvivesPropAvoidance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorSpeedSurvivesPropAvoidance::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	SpawnProp(World, FVector(400.f, 0.f, 150.f), FVector(1.f, 1.f, 1.f));

	AFishActor* Fish = SpawnFish(World, 5u);
	Fish->bPlayerControlled = true;
	Fish->PlaneOrigin = FVector(400.f, -250.f, 150.f);
	Fish->PlaneHalfWidth = 400.f;
	Fish->PlaneHalfHeight = 200.f;
	Fish->InitializeSwim();
	Fish->SetInputDirection(FVector2D(1.f, 0.f));
	for (int i = 0; i < 60; ++i) Fish->StepSwim(0.05f);   // reach cruising speed

	// This is the M3 regression guard: an avoidance rule that zeroes the blocked component makes
	// the speed dip toward zero, and a velocity through zero has no stable heading, which flipped
	// the facing 180 degrees. The speed must stay up the whole way past the prop.
	float MinSpeed = 1e9f;
	float MaxFacingStepDeg = 0.f;
	bool bPassedProp = false;
	FQuat Prev = Fish->GetActorQuat();
	for (int i = 0; i < 340; ++i)
	{
		// Stop before the far wall. The plan's version simply ran 340 more steps, which is 17 s
		// of travel across a 400 cm half-width plane: the fish parks against the boundary and
		// AvoidBoundary brings it to a standstill BY DESIGN (that is the M3 player rule -- push
		// into a wall and you stop). Measured min speed was 0.0 cm/s, a failure that said nothing
		// about prop avoidance. The window ends where the boundary band begins.
		if (Fish->GetActorLocation().Y - Fish->PlaneOrigin.Y > Fish->PlaneHalfWidth - 60.f)
		{
			break;
		}
		Fish->StepSwim(0.05f);
		if (Fish->GetActorLocation().Y > 30.f)
		{
			bPassedProp = true;   // the prop sits at Y = 0 with a 50 cm radius
		}
		MinSpeed = FMath::Min(MinSpeed, Fish->CurrentSpeed());
		const FQuat Now = Fish->GetActorQuat();
		MaxFacingStepDeg = FMath::Max(MaxFacingStepDeg, FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(Prev.GetAxisX(), Now.GetAxisX())), -1.f, 1.f))));
		Prev = Now;
	}
	AddInfo(FString::Printf(TEXT("min speed %.1f cm/s (max %.1f), largest nose swing %.1f deg"), MinSpeed, Fish->MaxSpeed, MaxFacingStepDeg));
	TestTrue(TEXT("the window actually covered the prop passage"), bPassedProp);
	TestTrue(FString::Printf(TEXT("speed never collapsed (min %.1f, floor %.1f)"), MinSpeed, Fish->MaxSpeed * 0.8f), MinSpeed > Fish->MaxSpeed * 0.8f);
	return TestTrue(FString::Printf(TEXT("no facing snap (largest swing %.1f deg)"), MaxFacingStepDeg), MaxFacingStepDeg < 45.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishSchoolDevTogglesParse, "Aquarium.Fish.DevTogglesParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishSchoolDevTogglesParse::RunTest(const FString&)
{
	// Pure parser, so no world and no command line are needed. These two flags exist for the
	// per-item performance attribution run: M4b showed that predicting which item is expensive
	// does not work, and toggling one at a time does.
	TestTrue(TEXT("recognises -AquariumNoSchooling"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumNoSchooling -other"), TEXT("AquariumNoSchooling")));
	TestTrue(TEXT("recognises -AquariumNoPropAvoid"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumNoPropAvoid"), TEXT("AquariumNoPropAvoid")));
	TestFalse(TEXT("absent flag is false"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumAutoInput=RRLL"), TEXT("AquariumNoSchooling")));
	TestFalse(TEXT("the two flags are independent"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumNoSchooling"), TEXT("AquariumNoPropAvoid")));
	TestFalse(TEXT("a null command line is false, not a crash"),
		UFishSchoolSubsystem::ParseDisableFlag(nullptr, TEXT("AquariumNoSchooling")));
	return TestFalse(TEXT("a null flag is false, not a crash"),
		UFishSchoolSubsystem::ParseDisableFlag(TEXT("Aquarium -AquariumNoSchooling"), nullptr));
}

// F-10/F-11: a clicked fish turns away from the touch point, speeds up, and comes back.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFleesFromTouch, "Aquarium.Fish.FleesFromTouchPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFleesFromTouch::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	Fish->InitializeSwim();
	// Let it get moving first, so the flee has to overcome a real velocity.
	for (int32 i = 0; i < 60; ++i) { Fish->StepSwim(1.f / 60.f); }

	// Touch 20 cm to the fish's screen-left: it must end up moving screen-right.
	const FVector Touch = Fish->GetActorLocation() - FVector(0.f, 20.f, 0.f);
	Fish->ApplyFleeFrom(Touch);
	TestTrue(TEXT("state is Fleeing right after the touch"),
		Fish->FleeState() == aquarium::BehaviorState::Fleeing);

	// The touch is head-on to this fish's heading, so the flee is a full reversal: it must END UP
	// moving screen-right (+Y) by the time the 0.8 s flee is over. Measured over two consecutive
	// frames so this reads the velocity, not a position that a reversal leaves almost unchanged.
	for (int32 i = 0; i < 47; ++i) { Fish->StepSwim(1.f / 60.f); }
	const double YNearEnd = Fish->GetActorLocation().Y;
	Fish->StepSwim(1.f / 60.f);
	TestTrue(TEXT("moving away from the touch (screen right = +Y)"),
		Fish->GetActorLocation().Y > YNearEnd);
	TestTrue(TEXT("touch really was on the fish's left"), Touch.Y < Fish->GetActorLocation().Y);
	// PLAN DEVIATION, see the report: the plan asserted "flee is faster than cruising" 0.5 s into
	// THIS flee, which is physically impossible here. A head-on reversal from 30 cm/s costs 0.75 s
	// at decel = 40 cm/s^2 before the speed can rise at all, and the whole flee lasts 0.8 s
	// (measured: speed fell 29.72 -> 24.74 over those 30 frames). The speed burst is therefore
	// checked on a second fish whose flee runs ALONG its heading, and against the fish's own
	// MaxSpeed -- a bound aquarium::StepMotion cannot cross unless FleeParams::fleeSpeedScale is
	// actually applied to MotionParamsValue.maxSpeed.
	{
		AFishActor* Runner = World->SpawnActor<AFishActor>();
		Runner->PlaneOrigin = FVector(400.f, 0.f, 100.f);
		Runner->InitializeSwim();
		for (int32 i = 0; i < 60; ++i) { Runner->StepSwim(1.f / 60.f); }
		const FVector Before = Runner->GetActorLocation();
		Runner->StepSwim(1.f / 60.f);
		const FVector Heading = (Runner->GetActorLocation() - Before).GetSafeNormal();
		const float RunnerCruise = Runner->CurrentSpeed();
		// Touch BEHIND the fish, so "away from the touch" is the way it is already going.
		Runner->ApplyFleeFrom(Runner->GetActorLocation() - Heading * 20.f);
		float Peak = 0.f;
		for (int32 i = 0; i < 48; ++i) { Runner->StepSwim(1.f / 60.f); Peak = FMath::Max(Peak, Runner->CurrentSpeed()); }
		TestTrue(FString::Printf(TEXT("flee is faster than cruising (%.2f > %.2f)"), Peak, RunnerCruise),
			Peak > RunnerCruise * 1.2f);
		TestTrue(FString::Printf(TEXT("flee exceeds the ordinary max speed (%.2f > %.2f)"), Peak, Runner->MaxSpeed),
			Peak > Runner->MaxSpeed);
	}
	for (int32 i = 0; i < 20; ++i) { Fish->StepSwim(1.f / 60.f); }   // total 0.833 s -> Recovering
	TestTrue(TEXT("recovering after 0.8 s"),
		Fish->FleeState() == aquarium::BehaviorState::Recovering);
	for (int32 i = 0; i < 80; ++i) { Fish->StepSwim(1.f / 60.f); }   // total 2.16 s -> Normal
	TestTrue(TEXT("normal after 2.0 s"),
		Fish->FleeState() == aquarium::BehaviorState::Normal);
	return true;
}

// F-07: the boundary rule still gets the last word while a fish is fleeing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFleeStaysInsideArea, "Aquarium.Fish.FleeStaysInsideArea",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFleeStaysInsideArea::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	Fish->PlaneHalfWidth = 60.f;
	Fish->PlaneHalfHeight = 40.f;
	Fish->InitializeSwim();
	// Click repeatedly from the opposite side so the flee direction always points at a wall.
	float WorstY = 0.f, WorstZ = 0.f;
	for (int32 i = 0; i < 600; ++i)
	{
		if (i % 120 == 0)
		{
			Fish->ApplyFleeFrom(Fish->GetActorLocation() - FVector(0.f, 30.f, 20.f));
		}
		Fish->StepSwim(1.f / 60.f);
		WorstY = FMath::Max(WorstY, static_cast<float>(FMath::Abs(Fish->GetActorLocation().Y - Fish->PlaneOrigin.Y)));
		WorstZ = FMath::Max(WorstZ, static_cast<float>(FMath::Abs(Fish->GetActorLocation().Z - Fish->PlaneOrigin.Z)));
	}
	// Derived from the actor's own half extents, never from a literal.
	TestTrue(FString::Printf(TEXT("stays inside half width (%.2f <= %.2f)"), WorstY, Fish->PlaneHalfWidth),
		WorstY <= Fish->PlaneHalfWidth + KINDA_SMALL_NUMBER);
	TestTrue(FString::Printf(TEXT("stays inside half height (%.2f <= %.2f)"), WorstZ, Fish->PlaneHalfHeight),
		WorstZ <= Fish->PlaneHalfHeight + KINDA_SMALL_NUMBER);
	return true;
}

// F-12: a background fish goes back to wandering, not to a frozen heading.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorBackgroundResumesWander, "Aquarium.Fish.BackgroundResumesWanderAfterFlee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorBackgroundResumesWander::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Ref = World->SpawnActor<AFishActor>();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	for (AFishActor* F : {Ref, Fish})
	{
		F->PlaneOrigin = FVector(400.f, 0.f, 100.f);
		F->Seed = 7;
		F->InitializeSwim();
	}
	Fish->ApplyFleeFrom(Fish->GetActorLocation() - FVector(0.f, 25.f, 0.f));
	for (int32 i = 0; i < 600; ++i) { Ref->StepSwim(1.f / 60.f); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("back to Normal"), Fish->FleeState() == aquarium::BehaviorState::Normal);
	// Both fish have the same seed and the same wander target sequence, so once the flee is over
	// the disturbed fish must be steering toward a live target again: its speed must be back to
	// the ordinary cruising band rather than stuck at the flee burst.
	TestTrue(FString::Printf(TEXT("speed back in the cruise band (%.2f vs %.2f)"),
		Fish->CurrentSpeed(), Ref->CurrentSpeed()),
		Fish->CurrentSpeed() <= Ref->CurrentSpeed() + 1.f);
	TestTrue(TEXT("still swimming, not stalled"), Fish->CurrentSpeed() > 1.f);
	return true;
}

// F-12: while fleeing, the flee beats the arrow keys; from recovery the CURRENT key applies.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorFleeBeatsArrowKeys, "Aquarium.Fish.FleeBeatsArrowKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorFleeBeatsArrowKeys::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(220.f, 0.f, 105.f);
	Fish->bIsPlayerFish = true;
	Fish->bPlayerControlled = true;
	Fish->InitializeSwim();
	// A control fish holding exactly the same key and never clicked. PLAN DEVIATION, see the
	// report: the plan asserted the clicked fish ends up RIGHT of where it was when clicked, which
	// no amount of correct wiring can produce. The flee is head-on to a held key here, and
	// aquarium::StepMotion turns the velocity at accel = 30 cm/s^2, so reversing a 40 cm/s leftward
	// velocity needs 1.33 s while the whole flee lasts 0.8 s. "The flee beats the key" is therefore
	// measured the only way physics allows: against a fish holding the SAME key with no click.
	AFishActor* Control = World->SpawnActor<AFishActor>();
	Control->PlaneOrigin = FVector(220.f, 0.f, 105.f);
	Control->bIsPlayerFish = true;
	Control->bPlayerControlled = true;
	Control->InitializeSwim();
	// The child is holding LEFT (screen -Y) the whole time.
	for (int32 i = 0; i < 30; ++i)
	{
		Fish->SetInputDirection(FVector2D(-1.f, 0.f));
		Control->SetInputDirection(FVector2D(-1.f, 0.f));
		Fish->StepSwim(1.f / 60.f);
		Control->StepSwim(1.f / 60.f);
	}
	TestEqual(TEXT("the two fish are in lockstep before the click"),
		Fish->GetActorLocation().Y, Control->GetActorLocation().Y, 1e-3);

	// The child clicks their OWN fish, on its left side, so the flee wants to go RIGHT.
	Fish->ApplyFleeFrom(Fish->GetActorLocation() - FVector(0.f, 15.f, 0.f));
	for (int32 i = 0; i < 48; ++i)
	{
		Fish->SetInputDirection(FVector2D(-1.f, 0.f));   // still held, as the controller would
		Control->SetInputDirection(FVector2D(-1.f, 0.f));
		Fish->StepSwim(1.f / 60.f);
		Control->StepSwim(1.f / 60.f);
	}
	TestTrue(FString::Printf(TEXT("flee wins over the held key (%.2f right of the unclicked control %.2f)"),
		Fish->GetActorLocation().Y, Control->GetActorLocation().Y),
		Fish->GetActorLocation().Y > Control->GetActorLocation().Y + 1.f);

	// Run to the end of the flee, then keep holding LEFT through recovery.
	for (int32 i = 0; i < 20; ++i) { Fish->SetInputDirection(FVector2D(-1.f, 0.f)); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("recovering"), Fish->FleeState() == aquarium::BehaviorState::Recovering);
	const double YAtRecovery = Fish->GetActorLocation().Y;
	for (int32 i = 0; i < 60; ++i) { Fish->SetInputDirection(FVector2D(-1.f, 0.f)); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("the held key steers again from recovery"), Fish->GetActorLocation().Y < YAtRecovery);
	return true;
}

// F-12: a key RELEASED during the flee must not come back to life at recovery.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishActorRecoveryUsesCurrentInput, "Aquarium.Fish.RecoveryUsesCurrentInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishActorRecoveryUsesCurrentInput::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AFishActor* Fish = World->SpawnActor<AFishActor>();
	Fish->PlaneOrigin = FVector(220.f, 0.f, 105.f);
	Fish->bIsPlayerFish = true;
	Fish->bPlayerControlled = true;
	Fish->InitializeSwim();
	Fish->SetInputDirection(FVector2D(0.f, 1.f));                    // holding UP
	for (int32 i = 0; i < 30; ++i) { Fish->StepSwim(1.f / 60.f); }
	Fish->ApplyFleeFrom(Fish->GetActorLocation() + FVector(0.f, 0.f, 15.f));  // flee downward
	// The child lets go during the flee.
	for (int32 i = 0; i < 60; ++i) { Fish->SetInputDirection(FVector2D::ZeroVector); Fish->StepSwim(1.f / 60.f); }
	TestTrue(TEXT("recovering"), Fish->FleeState() == aquarium::BehaviorState::Recovering);
	const float SpeedAtRecovery = Fish->CurrentSpeed();
	for (int32 i = 0; i < 72; ++i) { Fish->SetInputDirection(FVector2D::ZeroVector); Fish->StepSwim(1.f / 60.f); }
	TestTrue(FString::Printf(TEXT("coasts to a stop, no resurrected key (%.2f < %.2f)"),
		Fish->CurrentSpeed(), SpeedAtRecovery),
		Fish->CurrentSpeed() < SpeedAtRecovery * 0.5f);
	return true;
}

// F-09: exactly one fish per click, and it is the frontmost one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishClickPicksFrontmost, "Aquarium.Fish.ClickPicksFrontmostFish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishClickPicksFrontmost::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	TestNotNull(TEXT("subsystem"), School);

	// PLAN DEVIATION, see the report: the plan spawned mesh-less fish, but AsClickTarget derives
	// its half extents from the RENDERED bounds, so a fish with no mesh has zero extents and is
	// correctly invisible to a click. A real mesh is what makes this test exercise anything.
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Mesh)) return false;

	// Three fish stacked on the same screen point at different plane depths.
	TArray<AFishActor*> Fish;
	for (float Depth : {700.f, 330.f, 500.f})
	{
		AFishActor* F = World->SpawnActor<AFishActor>();
		F->PlaneOrigin = FVector(Depth, 0.f, 100.f);
		F->SetMesh(Mesh);
		F->InitializeSwim();
		School->Register(F);
		Fish.Add(F);
	}
	// A ray from the camera position straight at that point.
	FVector Hit = FVector::ZeroVector;
	AFishActor* Picked = School->PickFrontmostHit(FVector(0.f, 0.f, 100.f), FVector(1.f, 0.f, 0.f), Hit);
	TestTrue(TEXT("something was hit"), Picked != nullptr);
	// TestEqual is ambiguous for AFishActor* (plan error), so compare the pointers directly.
	TestTrue(TEXT("the frontmost plane wins"), Picked == Fish[1]);   // depth 330
	TestEqual(TEXT("hit point is on that plane"), static_cast<float>(Hit.X), 330.f, 0.1f);
	return true;
}

// F-09: empty water affects nothing at all.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFishClickOnEmptyWater, "Aquarium.Fish.ClickOnEmptyWaterHitsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFishClickOnEmptyWater::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UFishSchoolSubsystem* School = World->GetSubsystem<UFishSchoolSubsystem>();
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"));
	if (!TestNotNull(TEXT("SK_BlueTang loads"), Mesh)) return false;
	AFishActor* F = World->SpawnActor<AFishActor>();
	F->PlaneOrigin = FVector(400.f, 0.f, 100.f);
	F->SetMesh(Mesh);
	F->InitializeSwim();
	School->Register(F);

	// Sanity: the same ray aimed AT the fish does hit, so the miss below is about where the ray
	// points and not about a target list that is empty for some other reason.
	FVector AimedHit = FVector::ZeroVector;
	TestTrue(TEXT("a ray aimed at the fish does hit it"),
		School->PickFrontmostHit(FVector(0.f, 0.f, 100.f), FVector(1.f, 0.f, 0.f), AimedHit) == F);

	FVector Hit = FVector::ZeroVector;
	// 3 m above every fish: nothing to hit.
	AFishActor* Picked = School->PickFrontmostHit(FVector(0.f, 0.f, 400.f), FVector(1.f, 0.f, 0.f), Hit);
	TestNull(TEXT("empty water hits nothing"), Picked);
	TestTrue(TEXT("no fish was disturbed"), F->FleeState() == aquarium::BehaviorState::Normal);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
