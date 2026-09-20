#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntryWidgetMessages, "Aquarium.UI.EntryErrorMessagesAreKorean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEntryWidgetMessages::RunTest(const FString&)
{
	TestEqual(TEXT("empty"), UEntryWidget::MessageFor(EEntryError::Empty).ToString(), FString(TEXT("별명을 입력해 주세요")));
	TestEqual(TEXT("too long"), UEntryWidget::MessageFor(EEntryError::TooLong).ToString(), FString(TEXT("별명은 12자까지 쓸 수 있어요")));
	TestEqual(TEXT("invalid"), UEntryWidget::MessageFor(EEntryError::InvalidCharacter).ToString(), FString(TEXT("쓸 수 없는 글자가 있어요")));
	TestEqual(TEXT("no fish"), UEntryWidget::MessageFor(EEntryError::NoFishAvailable).ToString(), FString(TEXT("지금은 물고기가 없어요. 잠시 후 다시 시도해 주세요")));
	TestTrue(TEXT("none is empty"), UEntryWidget::MessageFor(EEntryError::None).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntryWidgetClassifies, "Aquarium.UI.EntryClassifiesNicknameErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEntryWidgetClassifies::RunTest(const FString&)
{
	TestEqual(TEXT("blank"), UEntryWidget::ClassifyNickname(TEXT("   ")), EEntryError::Empty);
	TestEqual(TEXT("13 hangul"), UEntryWidget::ClassifyNickname(TEXT("가나다라마바사아자차카타파")), EEntryError::TooLong);
	TestEqual(TEXT("newline"), UEntryWidget::ClassifyNickname(TEXT("니\n모")), EEntryError::InvalidCharacter);
	TestEqual(TEXT("ok"), UEntryWidget::ClassifyNickname(TEXT("  니모 ")), EEntryError::None);
	return true;
}

namespace
{
	// CreateWidget needs a world that resolves to a game instance. A Game-type world with a bare
	// (uninitialised) UGameInstance is enough: the widget only needs the outer chain, not a
	// running game. Teardown runs from the destructor so early returns cannot leak the context.
	struct FScopedWidgetWorld
	{
		UWorld* World = nullptr;
		UGameInstance* GameInstance = nullptr;

		FScopedWidgetWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			GameInstance = NewObject<UGameInstance>(GEngine);
			World->SetGameInstance(GameInstance);
			Context.OwningGameInstance = GameInstance;
		}

		~FScopedWidgetWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			GameInstance->MarkAsGarbage();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntryWidgetConstructs, "Aquarium.UI.EntryWidgetConstructsAndGuardsDoubleSubmit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEntryWidgetConstructs::RunTest(const FString&)
{
	FScopedWidgetWorld Scope;

	UEntryWidget* Widget = CreateWidget<UEntryWidget>(Scope.World, UEntryWidget::StaticClass());
	if (!TestNotNull(TEXT("widget created"), Widget))
	{
		return false;
	}
	Widget->TakeWidget(); // forces RebuildWidget
	TestNotNull(TEXT("root built"), Widget->WidgetTree ? Widget->WidgetTree->RootWidget.Get() : nullptr);

	UEditableTextBox* Input = Cast<UEditableTextBox>(Widget->WidgetTree->FindWidget(TEXT("Input")));
	UButton* Button = Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("EnterButton")));
	if (!TestNotNull(TEXT("input"), Input) || !TestNotNull(TEXT("button"), Button))
	{
		return false;
	}
	TestTrue(TEXT("input slate widget resolves"), Widget->GetInputSlateWidget() == Input->TakeWidget());

	// Double-submit guard: the second click lands while the first is in flight.
	int32 Submits = 0;
	Widget->OnSubmitted.BindLambda([&Submits](const FString&) { ++Submits; });
	Input->SetText(FText::FromString(TEXT("니모")));
	Button->OnClicked.Broadcast();
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("second click ignored while submitting"), Submits, 1);
	TestFalse(TEXT("button disabled while submitting"), Button->GetIsEnabled());
	Widget->ShowError(EEntryError::NoFishAvailable);
	TestTrue(TEXT("button re-enabled after error"), Button->GetIsEnabled());

	// Re-entrant case: the owner rejects synchronously inside the callback.
	Submits = 0;
	Widget->OnSubmitted.BindLambda([&Submits, Widget](const FString&)
	{
		++Submits;
		Widget->ShowError(EEntryError::NoFishAvailable);
	});
	Button->OnClicked.Broadcast();
	TestTrue(TEXT("button enabled after re-entrant error"), Button->GetIsEnabled());
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("subsequent click submits again"), Submits, 2);

	Widget->ResetForEntry();
	TestTrue(TEXT("reset clears input"), Input->GetText().IsEmpty());
	Widget->RemoveFromParent();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
