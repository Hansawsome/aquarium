#include "NameTagWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "UiFont.h"

void UNameTagWidget::SetName(const FText& InName)
{
	PendingName = InName;
	if (Label)
	{
		Label->SetText(PendingName);
	}
}

TSharedRef<SWidget> UNameTagWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameLabel"));
		Label->SetFont(FUiFont::Get(22));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Label->SetShadowOffset(FVector2D(1.f, 1.f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Label->SetJustification(ETextJustify::Center);
		Label->SetText(PendingName);
		WidgetTree->RootWidget = Label;
	}
	return Super::RebuildWidget();
}
