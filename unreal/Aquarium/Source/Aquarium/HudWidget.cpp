#include "HudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "UiFont.h"

#define LOCTEXT_NAMESPACE "Aquarium"

namespace
{
	constexpr int32 kLabelFontSize = 20;
	constexpr float kEdgeMargin = 16.f;
	const FMargin kLabelPadding(24.f, 6.f);
	// Translucent light button / dark label: legible over the underwater scene without hiding it.
	const FLinearColor kButtonTint(0.98f, 0.9f, 0.7f, 0.75f);
	const FLinearColor kLabelTint(0.05f, 0.05f, 0.08f);
}

void UHudWidget::HandleExitClicked()
{
	OnExit.ExecuteIfBound();
}

TSharedRef<SWidget> UHudWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		ExitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExitButton"));
		ExitButton->OnClicked.AddDynamic(this, &UHudWidget::HandleExitClicked);
		ExitButton->SetBackgroundColor(kButtonTint);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ExitLabel"));
		Label->SetFont(FUiFont::Get(kLabelFontSize));
		Label->SetColorAndOpacity(FSlateColor(kLabelTint));
		Label->SetJustification(ETextJustify::Center);
		Label->SetText(LOCTEXT("HudExit", "나가기"));
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(ExitButton->AddChild(Label)))
		{
			LabelSlot->SetPadding(kLabelPadding);
		}

		if (UCanvasPanelSlot* ButtonSlot = Root->AddChildToCanvas(ExitButton))
		{
			ButtonSlot->SetAnchors(FAnchors(1.f, 0.f));
			ButtonSlot->SetAlignment(FVector2D(1.f, 0.f));
			ButtonSlot->SetPosition(FVector2D(-kEdgeMargin, kEdgeMargin));
			ButtonSlot->SetAutoSize(true);
		}
	}
	return Super::RebuildWidget();
}

#undef LOCTEXT_NAMESPACE

bool UHudWidget::IsPointerOverExitButton() const
{
	return ExitButton != nullptr && ExitButton->IsHovered();
}
