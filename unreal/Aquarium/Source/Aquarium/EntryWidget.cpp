#include "EntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UiFont.h"
#include "aquarium/Nickname.h"

#include <string>

#define LOCTEXT_NAMESPACE "Aquarium"

EEntryError UEntryWidget::ClassifyNickname(const FString& Raw)
{
	const aquarium::NicknameResult Result = aquarium::ValidateNickname(std::string(TCHAR_TO_UTF8(*Raw)));
	if (Result.ok)
	{
		return EEntryError::None;
	}
	switch (Result.error)
	{
	case aquarium::NicknameError::Empty:   return EEntryError::Empty;
	case aquarium::NicknameError::TooLong: return EEntryError::TooLong;
	default:                               return EEntryError::InvalidCharacter;
	}
}

FText UEntryWidget::MessageFor(EEntryError Error)
{
	switch (Error)
	{
	case EEntryError::Empty:            return LOCTEXT("EntryEmpty", "별명을 입력해 주세요");
	case EEntryError::TooLong:          return LOCTEXT("EntryTooLong", "별명은 12자까지 쓸 수 있어요");
	case EEntryError::InvalidCharacter: return LOCTEXT("EntryInvalidChar", "쓸 수 없는 글자가 있어요");
	case EEntryError::NoFishAvailable:  return LOCTEXT("EntryNoFish", "지금은 물고기가 없어요. 잠시 후 다시 시도해 주세요");
	case EEntryError::None:
	default:                            return FText::GetEmpty();
	}
}

void UEntryWidget::ShowError(EEntryError Error)
{
	if (ErrorText)
	{
		ErrorText->SetText(MessageFor(Error));
	}
	SetSubmitting(false);
}

void UEntryWidget::ResetForEntry()
{
	if (Input)
	{
		Input->SetText(FText::GetEmpty());
	}
	if (ErrorText)
	{
		ErrorText->SetText(FText::GetEmpty());
	}
	SetSubmitting(false);
	if (Input)
	{
		Input->SetKeyboardFocus();
	}
}

void UEntryWidget::SetSubmitting(bool bBusy)
{
	if (EnterButton)
	{
		EnterButton->SetIsEnabled(!bBusy);
	}
}

void UEntryWidget::HandleEnterClicked()
{
	Submit();
}

void UEntryWidget::HandleTextCommitted(const FText& /*Text*/, ETextCommit::Type Method)
{
	if (Method == ETextCommit::OnEnter)
	{
		Submit();
	}
}

void UEntryWidget::Submit()
{
	// Double-submit guard (F-02): a submit is already in flight or the widget is not built.
	if (!EnterButton || !EnterButton->GetIsEnabled() || !Input)
	{
		return;
	}
	const FString Raw = Input->GetText().ToString();
	const EEntryError Error = ClassifyNickname(Raw);
	if (Error != EEntryError::None)
	{
		ShowError(Error);
		return;
	}
	if (ErrorText)
	{
		ErrorText->SetText(FText::GetEmpty());
	}
	SetSubmitting(true);
	OnSubmitted.ExecuteIfBound(Raw);
}

TSharedRef<SWidget> UEntryWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Dark translucent panel so the text stays legible over the underwater scene.
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Panel->SetPadding(FMargin(32.f));
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetAutoSize(true);
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Panel->SetContent(Column);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetFont(FUiFont::Get(36));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		Title->SetText(LOCTEXT("EntryTitle", "별명을 정하고 바다로 들어가요"));
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		}

		Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input"));
		{
			// UE 5.8 has no UEditableTextBox::SetFont; the font lives in the widget style's text style.
			FEditableTextBoxStyle Style = Input->GetWidgetStyle();
			Style.SetFont(FUiFont::Get(28));
			Input->SetWidgetStyle(Style);
		}
		Input->SetHintText(LOCTEXT("EntryHint", "별명을 입력하세요 (12자까지)"));
		Input->SetMinDesiredWidth(420.f);
		Input->OnTextCommitted.AddDynamic(this, &UEntryWidget::HandleTextCommitted);
		if (UVerticalBoxSlot* InputSlot = Column->AddChildToVerticalBox(Input))
		{
			InputSlot->SetHorizontalAlignment(HAlign_Center);
			InputSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}

		EnterButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EnterButton"));
		EnterButton->OnClicked.AddDynamic(this, &UEntryWidget::HandleEnterClicked);
		UTextBlock* EnterLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnterLabel"));
		EnterLabel->SetFont(FUiFont::Get(28));
		EnterLabel->SetJustification(ETextJustify::Center);
		EnterLabel->SetText(LOCTEXT("EntryEnter", "입장"));
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(EnterButton->AddChild(EnterLabel)))
		{
			LabelSlot->SetPadding(FMargin(48.f, 8.f));
		}
		if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(EnterButton))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		}

		ErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ErrorText"));
		ErrorText->SetFont(FUiFont::Get(22));
		ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f)));
		ErrorText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* ErrorSlot = Column->AddChildToVerticalBox(ErrorText))
		{
			ErrorSlot->SetHorizontalAlignment(HAlign_Center);
			ErrorSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		}
	}
	return Super::RebuildWidget();
}

#undef LOCTEXT_NAMESPACE
