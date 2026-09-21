#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AquariumTestWorld.h"

#include "BubbleSubsystem.h"
#include "Camera/CameraActor.h"
#include "CatchSubsystem.h"
#include "DiverPlayerController.h"
#include "FishActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactShakesTheCamera, "Aquarium.Impact.ShakesTheCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImpactShakesTheCamera::RunTest(const FString&)
{
	AquariumTest::FWorld W;
	ACameraActor* Cam = AquariumTest::SpawnDiverCamera(W.Get());
	if (!TestNotNull(TEXT("camera"), Cam)) return false;
	ADiverPlayerController* PC = W.Get()->SpawnActor<ADiverPlayerController>();
	if (!TestNotNull(TEXT("controller"), PC)) return false;
	const FVector Base = Cam->GetActorLocation();
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	if (!TestNotNull(TEXT("catch subsystem"), CatchSub)) return false;
	CatchSub->TriggerShakeForTest();
	PC->TickCameraShakeForTest(1.f / 120.f);
	TestTrue(TEXT("camera moved"), !Cam->GetActorLocation().Equals(Base, 0.05f));
	// 그리고 짧다. 끝나면 **정확히** 제자리다 -- 누적 오차가 남으면 카메라가
	// 한 판 내내 조금씩 흘러간다(P-06: 카메라는 절대 물고기를 따라가지 않는다).
	for (int32 i = 0; i < 200; ++i)
	{
		CatchSub->Tick(1.f / 120.f);              // 흔들림의 수명을 실제로 태운다
		PC->TickCameraShakeForTest(1.f / 120.f);
	}
	TestTrue(FString::Printf(TEXT("exactly back (drift %.6f cm)"),
			FVector::Dist(Cam->GetActorLocation(), Base)),
		Cam->GetActorLocation().Equals(Base, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImpactShakeIsShort, "Aquarium.Impact.ShakeIsShort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImpactShakeIsShort::RunTest(const FString&)
{
	// 0.35초를 넘으면 '흔들림'이 아니라 '지진'이고, 연타하는 아이가 멀미한다.
	AquariumTest::FWorld W;
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	if (!TestNotNull(TEXT("catch subsystem"), CatchSub)) return false;
	TestTrue(FString::Printf(TEXT("short (%.3f s)"), CatchSub->ShakeParams().duration),
		CatchSub->ShakeParams().duration <= 0.35f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWakeLeavesATrailWhenFast, "Aquarium.Impact.WakeLeavesATrailWhenFast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWakeLeavesATrailWhenFast::RunTest(const FString&)
{
	AquariumTest::FWorld W;
	UBubbleSubsystem* Bubbles = W.Get()->GetSubsystem<UBubbleSubsystem>();
	if (!TestNotNull(TEXT("bubbles"), Bubbles)) return false;
	AFishActor* Mine = W.BeginSession();
	if (!TestNotNull(TEXT("player fish"), Mine)) return false;
	Mine->SetInputDirection(FVector2D(0.f, 0.f));
	for (int32 i = 0; i < 120; ++i) { Mine->StepSwim(1.f / 60.f); Bubbles->Tick(1.f / 60.f); }
	TestEqual(TEXT("drifting leaves nothing"), Bubbles->WakeSpawnedTotal(), 0);
	Mine->SetInputDirection(FVector2D(1.f, 0.f));
	// **끝 속도가 아니라 최고 속도로 본다.** 2초를 달리면 물고기가 벽에 닿아
	// 다시 느려지므로, 마지막 프레임만 보면 구현이 옳아도 빨간불이 된다.
	float Peak = 0.f;
	for (int32 i = 0; i < 120; ++i)
	{
		Mine->StepSwim(1.f / 60.f);
		Bubbles->Tick(1.f / 60.f);
		Peak = FMath::Max(Peak, Mine->CurrentSpeed());
	}
	// 이 시험이 무언가를 보고 있다는 증거: 물고기는 실제로 문턱을 넘었다.
	TestTrue(FString::Printf(TEXT("the fish really is over the threshold (%.1f > %.1f)"),
			Peak, Mine->MaxSpeed * Bubbles->WakeSpeedFraction),
		Peak > Mine->MaxSpeed * Bubbles->WakeSpeedFraction);
	TestTrue(FString::Printf(TEXT("running leaves a trail (%d)"), Bubbles->WakeSpawnedTotal()),
		Bubbles->WakeSpawnedTotal() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWakeDoesNotRise, "Aquarium.Impact.WakeDoesNotRise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWakeDoesNotRise::RunTest(const FString&)
{
	// 난류 자국은 기포가 아니다. 위로 뜨면 그것은 '귀여운 방울'이고, 시나리오가
	// 이름 붙여 금지한 것이다.
	AquariumTest::FWorld W;
	UBubbleSubsystem* Bubbles = W.Get()->GetSubsystem<UBubbleSubsystem>();
	if (!TestNotNull(TEXT("bubbles"), Bubbles)) return false;
	Bubbles->SpawnWake(FVector(300.f, 0.f, 100.f), FVector2D(1.f, 0.f), 1u);
	TestEqual(TEXT("one wake is alive"), Bubbles->ActiveWakeCount(), 1);
	const float Z0 = Bubbles->HighestWakeZ();
	for (int32 i = 0; i < 20; ++i) { Bubbles->Tick(1.f / 60.f); }
	TestTrue(TEXT("still alive while we look"), Bubbles->ActiveWakeCount() > 0);
	TestTrue(FString::Printf(TEXT("did not rise (%.3f -> %.3f)"), Z0, Bubbles->HighestWakeZ()),
		Bubbles->HighestWakeZ() <= Z0 + 0.01f);
	// 그리고 제 수명으로 사라진다(화면 위까지 갈 일이 없으므로 기포의 규칙을 쓰지 않는다).
	for (int32 i = 0; i < 600; ++i) { Bubbles->Tick(1.f / 60.f); }
	TestEqual(TEXT("gone"), Bubbles->ActiveWakeCount(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
