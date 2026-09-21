#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AquariumTestWorld.h"

#include "FishActor.h"
#include "NameTagComponent.h"
#include "NameTagWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStampAttachesTheName, "Aquarium.Stamp.AttachesTheName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStampAttachesTheName::RunTest(const FString&)
{
	AquariumTest::FWorld W;
	AFishActor* Fish = AquariumTest::SpawnBackgroundFish(W.Get(), 3u);
	TestFalse(TEXT("not stamped"), Fish->IsStamped());
	UNameTagComponent* Tag = Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	if (!TestNotNull(TEXT("tag"), Tag)) return false;
	TestTrue(TEXT("stamped"), Fish->IsStamped());
	TestEqual(TEXT("the child's name"), Tag->DisplayedName().ToString(), FString(TEXT("민지")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStampIsQuieterThanMyOwnTag, "Aquarium.Stamp.IsQuieterThanMyOwnTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStampIsQuieterThanMyOwnTag::RunTest(const FString&)
{
	// 36마리가 전부 같은 크기의 흰 글자를 이고 다니면 화면이 글자밭이 된다.
	// 도장은 내 이름표보다 작고 옅다.
	AquariumTest::FWorld W;
	AFishActor* Fish = AquariumTest::SpawnBackgroundFish(W.Get(), 3u);
	UNameTagComponent* Tag = Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	if (!TestNotNull(TEXT("tag"), Tag)) return false;
	UNameTagWidget* Widget = Cast<UNameTagWidget>(Tag->GetUserWidgetObject());
	if (!TestNotNull(TEXT("widget"), Widget)) return false;
	// Slate가 위젯을 실제로 만들게 한다. 헤드리스에서는 아무도 가져가지 않아
	// RebuildWidget이 돌지 않고, 그러면 Label이 없어 '실제로 그려지는 값'을 볼 수 없다.
	Widget->TakeWidget();
	TestTrue(TEXT("stamp style is on"), Widget->IsStampStyle());
	TestTrue(TEXT("smaller than the owner tag"), Widget->FontSize() < UNameTagWidget::OwnerFontSize());
	TestTrue(TEXT("dimmer than the owner tag"), Widget->Opacity() < 1.f);
	// 그리고 **실제로 그려지는 값**이 따라왔는지. 상태 변수만 보면 글꼴 갱신을
	// 지우는 변이에 초록불인 채다(M7의 뮤트 버튼에서 있었던 일).
	TestEqual(TEXT("the label really is the smaller font"), Widget->AppliedFontSize(), UNameTagWidget::StampFontSize());
	TestTrue(FString::Printf(TEXT("the label really is dimmer (%.2f)"), Widget->AppliedOpacity()),
		Widget->AppliedOpacity() < 0.99f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStampCannotBeRemovedOneByOne, "Aquarium.Stamp.CannotBeRemovedOneByOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStampCannotBeRemovedOneByOne::RunTest(const FString&)
{
	// 두 번 찍어도 하나다. 그리고 떼는 공개 함수는 존재하지 않는다 -- 있는 것은
	// 세션 전체를 되돌리는 ClearStampForSessionReset()뿐이다.
	AquariumTest::FWorld W;
	AFishActor* Fish = AquariumTest::SpawnBackgroundFish(W.Get(), 3u);
	Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	TestTrue(TEXT("still stamped"), Fish->IsStamped());
	Fish->ClearStampForSessionReset();
	TestFalse(TEXT("gone after leaving"), Fish->IsStamped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStampFollowsTheFish, "Aquarium.Stamp.FollowsTheFish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStampFollowsTheFish::RunTest(const FString&)
{
	// 도장이 제자리에 남으면 '박혔다'가 아니라 '떨어졌다'로 보인다.
	AquariumTest::FWorld W;
	AFishActor* Fish = AquariumTest::SpawnBackgroundFish(W.Get(), 3u);
	UNameTagComponent* Tag = Fish->ApplyStamp(FText::FromString(TEXT("민지")));
	if (!TestNotNull(TEXT("tag"), Tag)) return false;
	const FVector Before = Tag->GetComponentLocation();
	const FVector FishBefore = Fish->GetActorLocation();
	// Tick이 StepSwim과 UpdateNameTagLocation을 둘 다 부른다. StepSwim만 부르면
	// 태그가 절대 좌표라 제자리에 남고, 이 테스트는 구현이 옳아도 빨간불이 된다.
	for (int32 i = 0; i < 120; ++i) { Fish->Tick(1.f / 60.f); }
	TestTrue(TEXT("the fish itself moved"), !Fish->GetActorLocation().Equals(FishBefore, 1.f));
	const FVector After = Tag->GetComponentLocation();
	TestTrue(TEXT("the mark moved with the fish"), !After.Equals(Before, 1.f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
