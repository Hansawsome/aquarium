#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AquariumTestWorld.h"

#include "Components/TextBlock.h"
#include "HudWidget.h"

namespace
{
UHudWidget* MakeHud(UWorld* World)
{
	UHudWidget* Hud = CreateWidget<UHudWidget>(World, UHudWidget::StaticClass());
	if (Hud)
	{
		Hud->TakeWidget();   // RebuildWidget을 실제로 돌린다
	}
	return Hud;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudShowsTheCount, "Aquarium.Hud.ShowsTheCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHudShowsTheCount::RunTest(const FString&)
{
	AquariumTest::FWorld W;
	UHudWidget* Hud = MakeHud(W.Get());
	if (!TestNotNull(TEXT("hud"), Hud)) return false;
	Hud->SetCatchCount(0);
	TestEqual(TEXT("starts at zero"), Hud->CountText(), FString(TEXT("0")));
	Hud->SetCatchCount(8);
	TestEqual(TEXT("shows eight"), Hud->CountText(), FString(TEXT("8")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudCountHasNoDenominatorOrLabel, "Aquarium.Hud.CountHasNoDenominatorOrLabel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHudCountHasNoDenominatorOrLabel::RunTest(const FString&)
{
	// "8 / 36"도 "잡은 수: 8"도 아니다. 분모는 목표 선언이고, 선언하는 순간
	// 유치해진다(시나리오: 완주 보상은 조용해야 한다).
	AquariumTest::FWorld W;
	UHudWidget* Hud = MakeHud(W.Get());
	if (!TestNotNull(TEXT("hud"), Hud)) return false;
	Hud->SetCatchCount(8);
	const FString Text = Hud->CountText();
	TestFalse(TEXT("no slash"), Text.Contains(TEXT("/")));
	TestFalse(TEXT("no words"), Text.Contains(TEXT("마리")));
	TestEqual(TEXT("digits only"), Text, FString(TEXT("8")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudCountDoesNotStealClicks, "Aquarium.Hud.CountDoesNotStealClicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHudCountDoesNotStealClicks::RunTest(const FString&)
{
	// 숫자가 클릭을 먹으면 그 자리의 물고기를 놀래킬 수 없다(F-09의 규칙).
	AquariumTest::FWorld W;
	UHudWidget* Hud = MakeHud(W.Get());
	if (!TestNotNull(TEXT("hud"), Hud)) return false;
	// 라벨이 정말 만들어졌는지 먼저 본다 -- 없으면 아래 단언은 nullptr을 보고
	// 통과하는 빈 테스트가 된다.
	TestFalse(TEXT("the label exists"), Hud->CountText().IsEmpty());
	TestFalse(TEXT("the count is not hit-testable"), Hud->CountIsHitTestable());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
