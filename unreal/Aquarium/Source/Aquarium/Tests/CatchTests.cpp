#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AquariumTestWorld.h"

#include "AquariumAudioSubsystem.h"
#include "AquariumGameMode.h"
#include "CatchSubsystem.h"
#include "FishActor.h"
#include "FishSchoolSubsystem.h"

#include "aquarium/Flee.h"

namespace
{
// 내 물고기를 원하는 방향으로 실제 평속까지 달리게 한다.
void RunPlayer(AFishActor* Mine, const FVector2D& Dir, int32 Steps = 60)
{
	Mine->SetInputDirection(Dir);
	for (int32 i = 0; i < Steps; ++i) { Mine->StepSwim(1.f / 60.f); }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchFastRamStamps, "Aquarium.Catch.FastRamStamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchFastRamStamps::RunTest(const FString&)
{
	AquariumTest::FWorld W;
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	if (!TestNotNull(TEXT("catch subsystem"), CatchSub)) return false;
	AFishActor* Mine = W.BeginSession();
	if (!TestNotNull(TEXT("player fish"), Mine)) return false;
	AFishActor* Target = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	if (!TestNotNull(TEXT("target"), Target)) return false;
	RunPlayer(Mine, FVector2D(1.f, 0.f));
	// 이 시험이 무언가를 보고 있다는 증거: 내 물고기는 실제로 빠르게 달리고 있다.
	TestTrue(TEXT("the player is actually running"), Mine->CurrentSpeed() > Mine->MaxSpeed * 0.8f);
	CatchSub->SetTargetUnderNoseForTest(Target, Mine);      // 코끝 화면 위치에 다시 맞춘다
	TestEqual(TEXT("nothing stamped yet"), CatchSub->StampCount(), 0);
	CatchSub->Tick(1.f / 60.f);
	TestEqual(TEXT("one stamp"), CatchSub->StampCount(), 1);
	TestTrue(TEXT("that fish carries the mark"), Target->IsStamped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchSlowDriftDoesNotStamp, "Aquarium.Catch.SlowDriftDoesNotStamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchSlowDriftDoesNotStamp::RunTest(const FString&)
{
	// 이 게임이 어렵다는 주장이 걸려 있는 테스트다. 느리게 겹쳐지는 것은 잡기가 아니다.
	AquariumTest::FWorld W;
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = W.BeginSession();
	if (!TestNotNull(TEXT("player fish"), Mine)) return false;
	AFishActor* Target = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	RunPlayer(Mine, FVector2D(0.f, 0.f), 240);             // 표류
	TestTrue(TEXT("barely moving"), Mine->CurrentSpeed() < Mine->MaxSpeed * 0.1f);
	CatchSub->SetTargetUnderNoseForTest(Target, Mine);
	CatchSub->Tick(1.f / 60.f);
	TestEqual(TEXT("no stamp"), CatchSub->StampCount(), 0);
	TestFalse(TEXT("not stamped"), Target->IsStamped());
	// 다만 아무 일도 없지는 않다: 놈은 놀라서 튄다. 그것이 다시 붙을 이유가 된다.
	TestTrue(TEXT("the target was startled"), Target->FleeState() != aquarium::BehaviorState::Normal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchCountsEachFishOnce, "Aquarium.Catch.CountsEachFishOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchCountsEachFishOnce::RunTest(const FString&)
{
	AquariumTest::FWorld W;
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = W.BeginSession();
	if (!TestNotNull(TEXT("player fish"), Mine)) return false;
	AFishActor* Target = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	RunPlayer(Mine, FVector2D(1.f, 0.f));
	for (int32 i = 0; i < 5; ++i)
	{
		CatchSub->SetTargetUnderNoseForTest(Target, Mine);
		CatchSub->Tick(1.f / 60.f);
	}
	TestEqual(TEXT("still one"), CatchSub->StampCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchNeverStampsMyOwnFish, "Aquarium.Catch.NeverStampsMyOwnFish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchNeverStampsMyOwnFish::RunTest(const FString&)
{
	AquariumTest::FWorld W;
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = W.BeginSession();
	if (!TestNotNull(TEXT("player fish"), Mine)) return false;
	// 내 물고기를 등록해 둔다 -- 실제 게임과 같은 상태다.
	W.Get()->GetSubsystem<UFishSchoolSubsystem>()->Register(Mine);
	// **그리고 진짜 표적도 하나 둔다.** 이것이 없으면 이 테스트는 빨간불이 될 수
	// 없다: 제외 검사를 지워도 내 물고기는 자기 자신에 대해 접근 속도가 0이라
	// Catch가 아니라 Bump가 되고, 숫자는 어차피 0인 채이기 때문이다(실제로
	// 변이 A가 초록불이었다). 제외 검사가 사라졌을 때 진짜로 망가지는 것은
	// **내 물고기(깊이 220)가 언제나 맨 앞엣놈이라 뒤의 표적을 통째로 가린다**는
	// 것이고, 그것은 표적이 있어야만 보인다.
	AFishActor* Target = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	RunPlayer(Mine, FVector2D(1.f, 0.f));
	CatchSub->SetTargetUnderNoseForTest(Target, Mine);
	for (int32 i = 0; i < 30; ++i) { CatchSub->Tick(1.f / 60.f); }
	TestFalse(TEXT("my fish is not stamped"), Mine->IsStamped());
	TestTrue(TEXT("the real target still got hit -- my fish did not shadow it"), Target->IsStamped());
	TestEqual(TEXT("exactly one, and it is not mine"), CatchSub->StampCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchNoticesBeforeContact, "Aquarium.Catch.NoticesBeforeContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchNoticesBeforeContact::RunTest(const FString&)
{
	// "놈이 먼저 눈치채고" -- 닿기 전에 회피가 걸려야 한다. 걸리지 않으면 이 게임은
	// 추격이 아니라 조준이 되고, 그러면 어렵지 않다.
	AquariumTest::FWorld W;
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = W.BeginSession();
	if (!TestNotNull(TEXT("player fish"), Mine)) return false;
	AFishActor* Target = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	RunPlayer(Mine, FVector2D(1.f, 0.f));
	// 코끝 앞쪽 0.08 화면 단위: 겹치지는 않되(상대 반지름 ~0.036) 눈치채는 반경
	// (EvadeParams::noticeRadius = 0.13) 안이다.
	CatchSub->SetTargetAheadForTest(Target, Mine, 0.08f);
	TestEqual(TEXT("nothing noticed yet"), Target->EvadeNoticeCount(), 0);
	CatchSub->Tick(1.f / 60.f);
	TestFalse(TEXT("not caught yet -- it is still out of reach"), Target->IsStamped());
	TestEqual(TEXT("and nothing was counted"), CatchSub->StampCount(), 0);
	TestTrue(TEXT("noticed"), Target->EvadeNoticeCount() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchPlaysThudOnlyOnACatch, "Aquarium.Catch.PlaysThudOnlyOnACatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchPlaysThudOnlyOnACatch::RunTest(const FString&)
{
	AquariumTest::FWorld W;
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	UAquariumAudioSubsystem* Audio = W.Get()->GetSubsystem<UAquariumAudioSubsystem>();
	if (!TestNotNull(TEXT("audio"), Audio)) return false;
	AFishActor* Mine = W.BeginSession();
	if (!TestNotNull(TEXT("player fish"), Mine)) return false;
	AFishActor* Target = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	RunPlayer(Mine, FVector2D(0.f, 0.f), 240);
	CatchSub->SetTargetUnderNoseForTest(Target, Mine);
	CatchSub->Tick(1.f / 60.f);
	TestEqual(TEXT("no thud on a bump"), Audio->CuePlayCount(EAquariumCue::Thud), 0);
	TestTrue(TEXT("but a bump is still an event"), Audio->CuePlayCount(EAquariumCue::Startle) > 0);
	RunPlayer(Mine, FVector2D(1.f, 0.f));
	CatchSub->SetTargetUnderNoseForTest(Target, Mine);
	CatchSub->Tick(1.f / 60.f);
	TestEqual(TEXT("thud on a catch"), Audio->CuePlayCount(EAquariumCue::Thud), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatchResetsOnLeaving, "Aquarium.Catch.ResetsOnLeaving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatchResetsOnLeaving::RunTest(const FString&)
{
	// 시나리오: 세션 동안 유지, 나가기로 리셋.
	AquariumTest::FWorld W;
	UCatchSubsystem* CatchSub = W.Get()->GetSubsystem<UCatchSubsystem>();
	AFishActor* Mine = W.BeginSession();
	if (!TestNotNull(TEXT("player fish"), Mine)) return false;
	AFishActor* Target = CatchSub->SpawnTargetUnderNoseForTest(Mine);
	RunPlayer(Mine, FVector2D(1.f, 0.f));
	CatchSub->SetTargetUnderNoseForTest(Target, Mine);
	CatchSub->Tick(1.f / 60.f);
	TestEqual(TEXT("one stamp"), CatchSub->StampCount(), 1);
	W.GameMode()->EndSession();
	TestEqual(TEXT("reset to zero"), CatchSub->StampCount(), 0);
	TestFalse(TEXT("the mark is gone from the fish too"), Target->IsStamped());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
