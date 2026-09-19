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

#endif // WITH_DEV_AUTOMATION_TESTS
