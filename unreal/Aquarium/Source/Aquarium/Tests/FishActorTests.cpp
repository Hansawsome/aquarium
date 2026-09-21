#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "FishActor.h"
#include "FishSchoolSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "aquarium/Boids.h"
#include "aquarium/Facing.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
