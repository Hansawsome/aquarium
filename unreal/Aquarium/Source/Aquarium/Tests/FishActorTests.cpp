#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "FishActor.h"

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

	// Bound: the facing slews at MaxFacingTurnRate (540 deg/s default) = 27 deg per 0.05 s step, so
	// 45 deg per frame must never be exceeded, even when the 2D velocity reverses through zero at a wall.
	constexpr float Dt = 0.05f;
	const float MaxTurnDegPerFrame = FMath::Min(45.f, Fish->MaxFacingTurnRate * Dt + 1.f);
	// The first step picks the initial heading from rest, so start measuring after it.
	Fish->StepSwim(Dt);
	FQuat PrevQuat = Fish->GetActorQuat();
	for (int i = 1; i < 300; ++i)
	{
		Fish->StepSwim(Dt);
		const FQuat Now = Fish->GetActorQuat();
		const float StepDeg = FMath::RadiansToDegrees(PrevQuat.AngularDistance(Now));
		if (!TestTrue(FString::Printf(TEXT("step %d: facing jumped %.1f deg (limit %.0f)"), i, StepDeg, MaxTurnDegPerFrame), StepDeg < MaxTurnDegPerFrame))
		{
			return false;
		}
		PrevQuat = Now;
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
