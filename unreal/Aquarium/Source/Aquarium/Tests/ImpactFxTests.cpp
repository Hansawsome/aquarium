#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AquariumTestWorld.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
