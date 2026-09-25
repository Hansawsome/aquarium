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

void UNameTagWidget::SetStampStyle(bool bInStamp)
{
	bStampStyle = bInStamp;
	if (Label)
	{
		Label->SetFont(FUiFont::Get(FontSize()));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, Opacity())));
	}
}

int32 UNameTagWidget::AppliedFontSize() const
{
	return Label ? Label->GetFont().Size : 0;
}

float UNameTagWidget::AppliedOpacity() const
{
	return Label ? Label->GetColorAndOpacity().GetSpecifiedColor().A : 0.f;
}

TSharedRef<SWidget> UNameTagWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameLabel"));
		Label->SetFont(FUiFont::Get(OwnerFontSize()));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Label->SetShadowOffset(FVector2D(1.f, 1.f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Label->SetJustification(ETextJustify::Center);
		Label->SetText(PendingName);
		WidgetTree->RootWidget = Label;
		// 위젯이 나중에 만들어져도 모양이 적용되도록(PendingName이 있는 이유와 같다).
		SetStampStyle(bStampStyle);
	}
	return Super::RebuildWidget();
}
