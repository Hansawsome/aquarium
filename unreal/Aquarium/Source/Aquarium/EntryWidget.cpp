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

namespace
{
	constexpr int32 kTitleFontSize = 36;
	constexpr int32 kBodyFontSize = 28;
	constexpr int32 kErrorFontSize = 26;
	constexpr int32 kErrorOutlineSize = 1;
	constexpr float kMinInputWidth = 420.f;
	constexpr float kPanelPadding = 32.f;
	constexpr float kTitleBottomPadding = 16.f;
	constexpr float kInputBottomPadding = 12.f;
	constexpr float kErrorTopPadding = 12.f;
	const FMargin kButtonLabelPadding(48.f, 8.f);
	const FLinearColor kPanelTint(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor kTitleTint = FLinearColor::White;
	const FLinearColor kButtonTint(0.98f, 0.9f, 0.7f);
	const FLinearColor kButtonLabelTint(0.05f, 0.05f, 0.08f);
	const FLinearColor kErrorTint(1.f, 0.85f, 0.3f);
	const FLinearColor kErrorOutlineTint(0.f, 0.f, 0.f, 0.8f);
}

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
}

void UEntryWidget::FocusInput()
{
	if (Input && IsVisible())
	{
		Input->SetKeyboardFocus();
	}
}

TSharedRef<SWidget> UEntryWidget::GetInputSlateWidget()
{
	// TakeWidget on the user widget guarantees the tree (and Input) exists.
	TakeWidget();
	return Input ? Input->TakeWidget() : SNullWidget::NullWidget;
}

FReply UEntryWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
	if (Input)
	{
		return FReply::Handled().SetUserFocus(Input->TakeWidget(), InFocusEvent.GetCause());
	}
	return FReply::Handled();
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
		SetIsFocusable(true);

		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		// Dark translucent panel so the text stays legible over the underwater scene.
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		Panel->SetBrushColor(kPanelTint);
		Panel->SetPadding(FMargin(kPanelPadding));
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetAutoSize(true);
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Panel->SetContent(Column);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetFont(FUiFont::Get(kTitleFontSize));
		Title->SetColorAndOpacity(FSlateColor(kTitleTint));
		Title->SetJustification(ETextJustify::Center);
		Title->SetText(LOCTEXT("EntryTitle", "별명을 정하고 바다로 들어가요"));
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, kTitleBottomPadding));
		}

		Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input"));
		{
			// UE 5.8 has no UEditableTextBox::SetFont; the font lives in the widget style's text style.
			FEditableTextBoxStyle Style = Input->GetWidgetStyle();
			Style.SetFont(FUiFont::Get(kBodyFontSize));
			Input->SetWidgetStyle(Style);
		}
		Input->SetHintText(LOCTEXT("EntryHint", "별명을 입력하세요 (12자까지)"));
		Input->SetMinDesiredWidth(kMinInputWidth);
		Input->OnTextCommitted.AddDynamic(this, &UEntryWidget::HandleTextCommitted);
		if (UVerticalBoxSlot* InputSlot = Column->AddChildToVerticalBox(Input))
		{
			InputSlot->SetHorizontalAlignment(HAlign_Center);
			InputSlot->SetPadding(FMargin(0.f, 0.f, 0.f, kInputBottomPadding));
		}

		EnterButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EnterButton"));
		EnterButton->OnClicked.AddDynamic(this, &UEntryWidget::HandleEnterClicked);
		// Explicit light button / dark label so contrast holds regardless of the default brush.
		EnterButton->SetBackgroundColor(kButtonTint);
		UTextBlock* EnterLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnterLabel"));
		EnterLabel->SetFont(FUiFont::Get(kBodyFontSize));
		EnterLabel->SetColorAndOpacity(FSlateColor(kButtonLabelTint));
		EnterLabel->SetJustification(ETextJustify::Center);
		EnterLabel->SetText(LOCTEXT("EntryEnter", "입장"));
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(EnterButton->AddChild(EnterLabel)))
		{
			LabelSlot->SetPadding(kButtonLabelPadding);
		}
		if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(EnterButton))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		}

		ErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ErrorText"));
		{
			FSlateFontInfo ErrorFont = FUiFont::Get(kErrorFontSize);
			ErrorFont.OutlineSettings = FFontOutlineSettings(kErrorOutlineSize, kErrorOutlineTint);
			ErrorText->SetFont(ErrorFont);
		}
		ErrorText->SetColorAndOpacity(FSlateColor(kErrorTint));
		ErrorText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* ErrorSlot = Column->AddChildToVerticalBox(ErrorText))
		{
			ErrorSlot->SetHorizontalAlignment(HAlign_Center);
			ErrorSlot->SetPadding(FMargin(0.f, kErrorTopPadding, 0.f, 0.f));
		}
	}
	return Super::RebuildWidget();
}

#undef LOCTEXT_NAMESPACE
