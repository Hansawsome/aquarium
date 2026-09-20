#include "NameTagComponent.h"

#include "NameTagWidget.h"

UNameTagComponent::UNameTagComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetWidgetClass(UNameTagWidget::StaticClass());
	SetRelativeLocation(FVector(0.f, 0.f, 20.f));
}

void UNameTagComponent::SetDisplayedName(const FText& InName)
{
	Name = InName;
	if (GetUserWidgetObject() == nullptr)
	{
		InitWidget();
	}
	if (UNameTagWidget* Widget = Cast<UNameTagWidget>(GetUserWidgetObject()))
	{
		Widget->SetName(Name);
	}
}
